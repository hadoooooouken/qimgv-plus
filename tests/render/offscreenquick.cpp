#include "offscreenquick.h"

#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderTarget>
#include <rhi/qrhi_platform.h>

namespace {
using namespace Qt::StringLiterals;
constexpr int kSingleSample = 1;
} // namespace

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

bool OffscreenQuick::create(QSize size) {
  mSize = size;
  QRhiD3D11InitParams params;
  mRhi.reset(QRhi::create(QRhi::D3D11, &params));
  if (!mRhi)
    return fail(u"Cannot create a Direct3D 11 QRhi"_s);

  mControl = std::make_unique<QQuickRenderControl>();
  mWindow = std::make_unique<QQuickWindow>(mControl.get());
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
