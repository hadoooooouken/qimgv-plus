attribute highp vec4 vertex;
attribute highp vec2 texCoordAttr;
varying highp vec2 texCoord;
void main() {
   gl_Position = vertex;
   texCoord = texCoordAttr;
}
