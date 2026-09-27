#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMatrix;
uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uColor;

out vec3 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos,1.0);
    vec3 normal = normalize(uNormalMatrix * aNormal);
    vec3 lightDir = normalize(uLightPos - vec3(worldPos));
    vec3 viewDir = normalize(uViewPos - vec3(worldPos));

    vec3 ambient = 0.15 * uColor;
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * uColor;

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal,halfwayDir), 0.0), 32.0);
    vec3 specular = 0.4 * spec * vec3(1.0);

    vColor = ambient + diffuse + specular;

    gl_Position = uProj * uView * worldPos;
}