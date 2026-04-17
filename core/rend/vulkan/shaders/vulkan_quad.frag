layout (set = 0, binding = 0) uniform sampler2D tex;

layout (push_constant) uniform pushBlock
{
	vec4 color;
} pushConstants;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 FragColor;

void main()
{
#if IGNORE_TEX_ALPHA == 1
	FragColor.rgb = pushConstants.color.rgb * texture(tex, inUV).rgb;
	FragColor.a = pushConstants.color.a;
#else
	FragColor = pushConstants.color * texture(tex, inUV);
#endif
}

