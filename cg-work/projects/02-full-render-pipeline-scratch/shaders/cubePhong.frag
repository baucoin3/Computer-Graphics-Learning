#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3 uColor;
uniform vec3 uLightPos;
uniform vec3 uViewPos;

out vec4 FragColor;

void main(){
    vec3 normal = normalize(vNormal); //N: unit surface normal after interpolation

    vec3 lightDir = normalize(uLightPos - vWorldPos); //L: direction to light 

    vec3 viewDir = normalize(uViewPos - vWorldPos); // V: direction to camera

    // ambient Ka * surfaceColor
    vec3 ambient = 0.15 * uColor;

    // diffuse = max(dot(N, L), 0) * surfaceColor
    float diff = max(dot(normal, lightDir), 0.0); //cos(theta) between N and L

    vec3 diffuse = diff * uColor;

    //specular = Ks * max*dot(N,H), 0)^ shininess * lightColor
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = 0.4 * spec * vec3(1.0);

    // final = ambient + diffuse + specular
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}