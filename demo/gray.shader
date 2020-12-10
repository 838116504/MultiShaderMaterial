shader_type canvas_item;

void fragment()
{
	vec4 c = texture(TEXTURE, UV);
	float gray = 0.2989 * c.r + 0.5870 * c.g + 0.1140 * c.b;
	COLOR = vec4(gray, gray, gray, c.a);
}