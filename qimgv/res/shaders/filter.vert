attribute highp vec4 vertex;
attribute highp vec2 texCoordAttr;
varying highp vec2 texCoord;
uniform highp mat4 matrix;
void main() {
   gl_Position = matrix * vertex;
   texCoord = texCoordAttr;
}
