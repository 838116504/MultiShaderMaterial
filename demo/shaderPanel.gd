extends Panel


func _on_shadersHbox_sort_children():
	rect_min_size = $shadersHbox.get_combined_minimum_size()
