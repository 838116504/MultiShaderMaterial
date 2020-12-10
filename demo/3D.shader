shader_type spatial;
render_mode blend_sub;

void vertex()
{
	VERTEX += vec3(100, 100, 0.0);
}

void fragment()
{
	ALBEDO *= (COLOR * 1.3).rgb;
}

void light()
{
	DIFFUSE_LIGHT += clamp(dot(NORMAL, LIGHT), 0.0, 1.0) * ATTENUATION * ALBEDO;
}