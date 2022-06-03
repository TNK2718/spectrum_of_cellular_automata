#version 330

// Varying変数
flat in vec3 f_fragColor;


// ディスプレイへの出力変数
out vec4 out_color;

void main() {
    out_color = vec4(f_fragColor, 1.0);
}
