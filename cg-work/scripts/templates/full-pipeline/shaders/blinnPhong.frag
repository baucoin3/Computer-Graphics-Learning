#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3  uColor;
uniform vec3  uLightPos;
uniform vec3  uLightColor;
uniform vec3  uViewPos;
uniform float uAmbient;
uniform float uSpecular;
uniform float uShininess;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos  - vWorldPos);
    vec3 V = normalize(uViewPos   - vWorldPos);
    vec3 H = normalize(L + V);

    vec3 ambient  = uAmbient * uColor;
    vec3 diffuse  = max(dot(N, L), 0.0) * uColor;
    vec3 specular = uSpecular * pow(max(dot(N, H), 0.0), uShininess) * uLightColor;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
