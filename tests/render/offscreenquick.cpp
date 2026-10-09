#include "offscreenquick.h"

#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderTarget>
#include <rhi/qrhi_platform.h>

namespace {
using namespace Qt::StringLiterals;
constexpr int kSingleSample = 1;
constexpr char kBackendVariable[] = "QSG_RHI_BACKEND";

struct BackendName {
  QRhi::Implementation backend;
  QLatin1StringView name;
};
constexpr BackendName kBackendNames[] = {
    {QRhi::D3D11, "d3d11"_L1},
    {QRhi::D3D12, "d3d12"_L1},
    {QRhi::Vulkan, "vulkan"_L1},
};
} // namespace

std::optional<QRhi::Implementation> testRhiBackend() {
  const QString requested =
      qEnvironmentVariable(kBackendVariable).trimmed().toLower();
  if (requested.isEmpty())
    return QRhi::D3D11;
  for (const BackendName &entry : kBackendNames) {
    if (requested == entry.name)
      return entry.backend;
  }
  return std::nullopt;
}

QString rhiBackendName(QRhi::Implementation backend) {
  for (const BackendName &entry : kBackendNames) {
    if (entry.backend == backend)
      return entry.name;
  }
  return u"unsupported"_s;
}

OffscreenQuick::~OffscreenQuick() {
  // The window's scene graph uses the render control and the QRhi; release
  // it first, then the render target, then the QRhi.
  mWindow.reset();
  mControl.reset();
  mRenderTarget.reset();
  mRenderPass.reset();
  mDepthStencil.reset();
  mColorBuffer.reset();
  mRhi.reset();
  mVulkanInstance.reset();
}

bool OffscreenQuick::fail(const QString &message) {
  mError = message;
  return false;
}

const QString &OffscreenQuick::error() const { return mError; }

QRhi *OffscreenQuick::rhi() const { return mRhi.get(); }

QQuickItem *OffscreenQuick::contentItem() const {
  return mWindow ? mWindow->contentItem() : nullptr;
}

QQuickWindow *OffscreenQuick::window() const {
  return mWindow.get();
}

bool OffscreenQuick::create(QSize size, QRhi::Implementation backend) {
  mSize = size;
  switch (backend) {
  case QRhi::D3D11: {
    QRhiD3D11InitParams params;
    mRhi.reset(QRhi::create(QRhi::D3D11, &params));
    break;
  }
  case QRhi::D3D12: {
    QRhiD3D12InitParams params;
    mRhi.reset(QRhi::create(QRhi::D3D12, &params));
    break;
  }
  case QRhi::Vulkan: {
    mVulkanInstance = std::make_unique<QVulkanInstance>();
    mVulkanInstance->setExtensions(
        QRhiVulkanInitParams::preferredInstanceExtensions());
    if (!mVulkanInstance->create())
      return fail(u"Cannot create a Vulkan instance (the offscreen QPA "
                  u"platform has no Vulkan support; use QT_QPA_PLATFORM=windows)"_s);
    QRhiVulkanInitParams params;
    params.inst = mVulkanInstance.get();
    mRhi.reset(QRhi::create(QRhi::Vulkan, &params));
    break;
  }
  default:
    return fail(u"Unsupported QRhi backend %1"_s.arg(int(backend)));
  }
  if (!mRhi)
    return fail(u"Cannot create a %1 QRhi"_s.arg(rhiBackendName(backend)));

  mControl = std::make_unique<QQuickRenderControl>();
  mWindow = std::make_unique<QQuickWindow>(mControl.get());
  if (mVulkanInstance)
    mWindow->setVulkanInstance(mVulkanInstance.get());
  mWindow->setGraphicsDevice(QQuickGraphicsDevice::fromRhi(mRhi.get()));
  if (!mControl->initialize())
    return fail(u"QQuickRenderControl::initialize() failed"_s);

  mColorBuffer.reset(mRhi->newTexture(
      QRhiTexture::RGBA8, size, kSingleSample,
      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
  if (!mColorBuffer->create())
    return fail(u"Cannot create the colour buffer"_s);
  mDepthStencil.reset(mRhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                            size, kSingleSample));
  if (!mDepthStencil->create())
    return fail(u"Cannot create the depth-stencil buffer"_s);
  QRhiTextureRenderTargetDescription description{
      QRhiColorAttachment(mColorBuffer.get())};
  description.setDepthStencilBuffer(mDepthStencil.get());
  mRenderTarget.reset(mRhi->newTextureRenderTarget(description));
  mRenderPass.reset(mRenderTarget->newCompatibleRenderPassDescriptor());
  mRenderTarget->setRenderPassDescriptor(mRenderPass.get());
  if (!mRenderTarget->create())
    return fail(u"Cannot create the render target"_s);

  mWindow->setRenderTarget(
      QQuickRenderTarget::fromRhiRenderTarget(mRenderTarget.get()));
  mWindow->setGeometry(0, 0, size.width(), size.height());
  mWindow->contentItem()->setSize(QSizeF(size));
  return true;
}

QImage OffscreenQuick::render() {
  if (!mControl || !mRhi) {
    fail(u"The offscreen scene was not created"_s);
    return {};
  }
  mControl->polishItems();
  mControl->beginFrame();
  mControl->sync();
  mControl->render();

  QRhiReadbackResult readback;
  QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
  if (!updates) {
    mControl->endFrame();
    fail(u"No resource update batch for the readback"_s);
    return {};
  }
  updates->readBackTexture(mColorBuffer.get(), &readback);
  mControl->commandBuffer()->resourceUpdate(updates);
  // Offscreen frames complete synchronously: the readback is done here.
  mControl->endFrame();

  if (readback.data.isEmpty() || readback.pixelSize != mSize) {
    fail(u"The colour buffer readback returned no data"_s);
    return {};
  }
  const QImage wrapper(reinterpret_cast<const uchar *>(readback.data.constData()),
                       mSize.width(), mSize.height(),
                       QImage::Format_RGBA8888_Premultiplied);
  QImage frame = wrapper.copy();
  if (mRhi->isYUpInFramebuffer())
    frame.flip(Qt::Vertical);
  return frame;
}
