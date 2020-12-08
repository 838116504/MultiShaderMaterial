# MultiShaderMaterial
This material combin multiple shaders.

# Installing
Put modules/multi_shader_material folder into Godot source modules folder.

If you want can new it on 2D node Inspector dock.

Go to scene/2d/canvas_item.cpp > CanvasItem::_bind_methods method, change below code

```C++
ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,CanvasItemMaterial"), "set_material", "get_material");
```

to

```C++
ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,CanvasItemMaterial,MultiShaderMaterial"), "set_material", "get_material");
```

# Principle
Shader type(2D, 3D, particle) depends on the first one in valid shaders. Other shaders that type is not same don't use. The render mode depends on the last one in using shaders. If only have one using shader, it will be used directly. If have more than one using shader, it will create new shader that combin the using shaders code. The combination method is rename the three entry functions'(vertex, fragment, light) build in variables with add prefix p_, rename all functions and variables name with add prefix [i]_[code]shader.get_instance_id()[/code]_[code]repeat id[/code]_[/i], add using entry functions code that call corresponding entry function.

# document
You can find it in Godot Editor by Ctrl + click class name in gdscript code.
