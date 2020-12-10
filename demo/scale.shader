shader_type canvas_item;

uniform float scaleRatio = 0.8;

void vertex()
{
	VERTEX *= scaleRatio;
}