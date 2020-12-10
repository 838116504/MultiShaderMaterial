extends HBoxContainer

export var buttonGroup:ButtonGroup
var tempShaderNode = null
var testMaterial = MultiShaderMaterial.new()

func get_selected_btn():
	if buttonGroup:
		return buttonGroup.get_pressed_button()
	return null

func _get_shader_id_by_local_pos(p_pos:Vector2) -> int:
	var pos:int = 0
	for i in get_children():
		if i.rect_position.x + i.rect_size.x < p_pos.x:
			pos += 1
		else:
			break
	return pos

func _gui_input(p_event):
	if p_event is InputEventMouseMotion && get_rect().has_point(p_event.position):
		var btn = get_selected_btn()
		if btn:
			var pos = _get_shader_id_by_local_pos(p_event.position)
			
			if !tempShaderNode:
				tempShaderNode = btn.create()
				if !tempShaderNode:
					return
				add_child(tempShaderNode)
				if pos != tempShaderNode.get_index():
					move_child(tempShaderNode, pos)
					if tempShaderNode.get_index() > 0:
						tempShaderNode.get_swap_btn().show()
					else:
						tempShaderNode.get_swap_btn().hide()
				tempShaderNode.set_target_material(testMaterial)
			else:
				if pos != tempShaderNode.get_index() && pos != tempShaderNode.get_index() + 1:
					move_child(tempShaderNode, pos)
					if tempShaderNode.get_index() > 0:
						tempShaderNode.get_swap_btn().show()
					else:
						tempShaderNode.get_swap_btn().hide()
	if p_event is InputEventMouseButton:
		if p_event.button_index == BUTTON_LEFT:
			if !p_event.pressed:
				tempShaderNode = null
				var btn = get_selected_btn()
				if btn:
					btn.pressed = false
		elif p_event.button_index == BUTTON_RIGHT:
			if !p_event.pressed:
				if tempShaderNode:
					tempShaderNode.queue_free()
					tempShaderNode = null
					var btn = get_selected_btn()
					if btn:
						btn.pressed = false
				elif get_child_count() > 0:
					var pos = _get_shader_id_by_local_pos(p_event.position)
					var child = get_child(pos)
					remove_child(child)
					child.queue_free()

func _notification(what):
	if what == NOTIFICATION_MOUSE_EXIT && !get_global_rect().has_point(get_global_mouse_position()):
		if (tempShaderNode):
			remove_child(tempShaderNode)
			tempShaderNode.queue_free()
			tempShaderNode = null
