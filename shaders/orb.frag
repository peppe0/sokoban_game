#version 330 core
// Fragment shader of the magic bolt. No texture at all: the look is computed
// from the two things the procedural mesh provides, the normal and the (theta,
// phi) parametrization stored as the UV.
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

uniform vec3  coreColor;   // colour at the centre of the disc
uniform vec3  rimColor;    // colour at the silhouette
uniform vec3  viewPos;     // camera position, in world space
uniform float time;        // seconds since the bolt was fired
uniform float intensity;   // overall brightness of this pass

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    // Fresnel term: 0 where the surface faces the camera, 1 at the silhouette
    // where the normal is perpendicular to the view direction. It is what turns
    // a flat-shaded ball into something that glows from the edges inward, and it
    // is also why the sphere has to carry real normals.
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.5);

    // Two sine waves running along the sphere's own parametrization, drifting in
    // opposite directions: the surface keeps churning although the mesh is
    // rigid. u wraps every 2*PI, hence the 12.566 = 4*PI on a 0..1 coordinate.
    float swirl = 0.5 + 0.5 * sin(TexCoords.x * 12.566 + time * 3.0)
                            * sin(TexCoords.y *  9.425 - time * 2.0);

    vec3 color = mix(coreColor, rimColor, fresnel);
    color *= intensity * (0.75 + 0.35 * swirl);

    // Drawn with additive blending, where alpha scales the contribution: giving
    // the rim more alpha makes the silhouette fade out instead of ending on a
    // hard circle.
    FragColor = vec4(color, 0.55 + 0.45 * fresnel);
}
