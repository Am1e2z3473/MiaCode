#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 baseColor;
    vec4 surfaceColor;
    float nativeMaterial;
    vec4 nativeTintColor;
    vec4 panelBaseColor;
};
layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D panelSource;

void main()
{
    vec4 wallpaper = texture(source, qt_TexCoord0);
    vec4 backing = wallpaper + baseColor * (1.0 - wallpaper.a);
    backing = surfaceColor + backing * (1.0 - surfaceColor.a);

    float distanceToCenter = length(qt_TexCoord0 - vec2(1.0));
    float edgeWidth = fwidth(distanceToCenter);
    float coverage = smoothstep(1.0 - edgeWidth * 0.5,
                               1.0 + edgeWidth * 0.5, distanceToCenter);
    if (nativeMaterial > 0.5) {
        vec4 panel = texture(panelSource, qt_TexCoord0);
        panel += panelBaseColor * (1.0 - panel.a);
        fragColor = mix(panel, nativeTintColor, coverage) * qt_Opacity;
    } else {
        fragColor = backing * coverage * qt_Opacity;
    }
}
