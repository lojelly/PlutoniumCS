#include "vibrant_logs.h"
#include "dynamic_array_spellbook.h"
#include "plutonium_cs.h"

// array of void*
typedef struct obj_arr
{
	void **data;
	size_t size, capacity;
} obj_arr;

// info for each component TYPE in the system
typedef struct component_type_info
{
	// array of object pointers (match-to-instances)
	obj_arr objs;
	// instances of this component
	obj_arr components;
	// size of component in bytes
	size_t elem_size;
	// init function (constructor-like)
	pluto_cs_init_fn init;
	// clone function
	pluto_cs_clone_fn clone;
	// type of component
	int type;
} component_type_info;

// array of component type info structs
typedef struct component_type_info_arr
{
	// array containing component info
	component_type_info *data;
	size_t size, capacity;
} component_type_info_arr;

// component header information
typedef struct component_header
{
	void *owner;
	int type;
} component_header;


static component_type_info_arr component_types = {0};
static bool init = false;


int pluto_cs_init()
{
	if(init)
	{
		vl_log(VL_ERROR, "Cannot re-initialize PlutoniumCS!\n");
		return 0;
	}

	dynas_init(&component_types);
	if(!component_types.data)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for component registry!\n");
		return 0;
	}

	init = true;
	vl_log(VL_SUCCESS, "Initialized PlutoniumCS!\n");
	return 1;
}
void pluto_cs_shutdown()
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot shutdown PlutoniumCS because it was never initialized!\n");
		return;
	}

	// free each component array for each type
	for(size_t i = 0; i < component_types.size; ++i)
	{
		// get type info at i
		component_type_info *info = &component_types.data[i];

		// free each component in the instances array:
		for(size_t j = 0; j < info->components.size; ++j)
		{
			void *component = info->components.data[j];
			component_header *header = ((component_header*) component) - 1;
			free(header);
		}

		// free the whole instances array
		dynas_free(&info->components);
		dynas_free(&info->objs);
	}

	// free the whole component type array
	dynas_free(&component_types);

	init = false;
	vl_log(VL_SUCCESS, "PlutoniumCS properly shut down!\n");
}

// get component type info based on a type
static component_type_info *find_component_type(int type)
{
	for(size_t i = 0; i < component_types.size; ++i)
	{
		if(component_types.data[i].type == type)
			return &component_types.data[i];
	}

	return NULL;
}

int pluto_cs_register(int type, size_t size_bytes, pluto_cs_init_fn init_fn, pluto_cs_clone_fn clone_fn)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot register component, PlutoniumCS was never initialized!\n");
		return 0;
	}
	if(!init_fn)
	{
		vl_log(VL_ERROR, "Registering a component requires the init function to be a valid pointer!\n");
		return 0;
	}

	// see if type already registered
	if(find_component_type(type))
	{
		vl_log(VL_ERROR, "Component type already registered: %d\n", type);
		return 0;
	}

	// type does not exist yet, so add it and track it
	component_type_info info = {
		.elem_size = size_bytes,
		.init = init_fn,
		.clone = clone_fn,
		.type = type,
	};

	// init the object arrays in this type info
	dynas_init(&info.components);
	if(!info.components.data)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for component %d!\n", type);
		return 0;
	}
	dynas_init(&info.objs);
	if(!info.objs.data)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for component %d!\n", type);
		return 0;
	}

	// check for bad alloc
	if(!info.components.data)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for component array: type = %d!\n", type);
		return 0;
	}
	if(!info.objs.data)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for object array: type = %d!\n", type);
		return 0;
	}

	dynas_add(&component_types, info);
	if(!component_types.data)
	{
		vl_log(VL_ERROR, "Failed to realloc memory in component system!\n");
		return 0;
	}

	vl_log(VL_SUCCESS, "Successfully registered component type: %d\n", type);

	return 1;
}

void *pluto_cs_add_component(void *obj, int type)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot add component, PlutoniumCS was never initialized!\n");
		return NULL;
	}
	if(!obj)
	{
		vl_log(VL_ERROR, "Cannot add component to a null object!\n");
		return NULL;
	}

	// see if component type registered
	component_type_info *type_info = find_component_type(type);
	if(!type_info)
	{
		vl_log(VL_ERROR, "Component type was never registered: %d!\n", type);
		return NULL;
	}

	// see if obj already contains the component type
	if(pluto_cs_check_component(obj, type))
	{
		vl_log(VL_ERROR, "This object %p already contains this component type: %d\n", obj, type);
		return NULL;
	}

	// init instance of type
	size_t component_malloc_size = sizeof(component_header) + type_info->elem_size;
	component_header *component = calloc(1, component_malloc_size);

	component->owner = obj;
	component->type = type;

	if(!component)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for component %d!\n", type);
		return NULL;
	}

	// add allocated pointer into instance array
	dynas_add(&type_info->components, component);
	if(!type_info->components.data)
	{
		vl_log(VL_ERROR, "Failed memory reallocation for component %d!\n", type);
		return NULL;
	}
	dynas_add(&type_info->objs, obj);
	if(!type_info->objs.data)
	{
		vl_log(VL_ERROR, "Failed memory rellocation for component %d!\n", type);
		return NULL;
	}

	// component was successfully allocated, init it
	type_info->init(component, obj);

	vl_log(VL_SUCCESS, "Added component %d to obj %p\n", type, obj);

	return component + 1;
}

int pluto_cs_remove_component(void *obj, int type)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot remove component, PlutoniumCS was never initialized!\n");
		return 0;
	}
	if(!obj)
	{
		vl_log(VL_ERROR, "Cannot remove component from null object!\n");
		return 0;
	}

	// see if type registered
	component_type_info *type_info = find_component_type(type);
	if(!type_info)
	{
		vl_log(VL_ERROR, "Component type was never registered: %d!\n", type);
		return 0;
	}

	// check to see if obj has the component
	if(!pluto_cs_check_component(obj, type))
	{
		vl_log(VL_ERROR, "Cannot remove component %d from object %p because it doesn't own the component.\n", type, obj);
		return 0;
	}

	// get component from object
	void *component = pluto_cs_get_component(obj, type);

	// remove component from array
	dynas_remove_item(&type_info->components, component);
	dynas_remove_item(&type_info->objs, obj);

	// free component (was allocated in add_component(...))
	component_header *header = ((component_header*) component) - 1;
	free(header);

	vl_log(VL_SUCCESS, "Removed component %d from obj %p!\n", type, obj);

	return 1;
}

bool pluto_cs_check_component(const void *const obj, int type)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot check for a component, PlutoniumCS was never initialized!\n");
		return false;
	}
	if(!obj)
	{
		vl_log(VL_ERROR, "Cannot check for a component on a null object!\n");
		return false;
	}

	// if obj was never registered, automatic false
	component_type_info *info = find_component_type(type);
	if(!info)
		return false;

	// see if object is paired with an instance of this component
	for(size_t i = 0; i < info->components.size; ++i)
	{
		// match type of the current instance with pointer in the instance
		if(info->type == type && info->objs.data[i] == obj)
			return true;
	}

	return false;
}

void *pluto_cs_get_component(const void *const obj, int type)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot obtain component, PlutoniumCS was never initialized!\n");
		return NULL;
	}
	if(!obj)
	{
		vl_log(VL_ERROR, "Cannot obtain a component from a null object!\n");
		return NULL;
	}
	
	// if obj was never registered, automatic false
	component_type_info *info = find_component_type(type);
	if(!info)
		return NULL;

	// find pair of obj-component
	for(size_t i = 0; i < info->components.size; ++i)
	{
		// if type and object pointer match, return that component
		if(info->type == type && info->objs.data[i] == obj)
			return info->components.data[i];
	}

	return NULL;
}

void *pluto_cs_get_owner(const void *const component)
{
	if(!component)
	{
		vl_log(VL_ERROR, "Cannot obtain owner of null component!\n");
		return NULL;
	}

	component_header *header = ((component_header*) component) - 1;

	return header->owner;
}
int pluto_cs_get_type(const void *const component)
{
	if(!component)
	{
		vl_log(VL_ERROR, "Cannot obtain owner of null component!\n");
		return PLUTO_CS_INVALID_COMPONENT;
	}

	component_header *header = ((component_header*) component) - 1;

	return header->type;
}

int pluto_cs_clone_single_component(const void *const src, void *target, int type)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot clone component, PlutoniumCS was never initialized!\n");
		return 0;
	}
	if(!src)
	{
		vl_log(VL_ERROR, "Cannot clone component from a null source object!\n");
		return 0;
	}
	if(!target)
	{
		vl_log(VL_ERROR, "Cannot add cloned component to a null target!\n");
		return 0;
	}

	vl_log(VL_INFO, "Cloning single component %d on source object %p:\n", type, src);

	// find instance of 'src' with the given component type
	for(size_t i = 0; i < component_types.size; ++i)
	{
		// get info at i
		component_type_info *info = &component_types.data[i];

		// now iterate over obj-component arrays
		for(size_t j = 0; j < info->components.size; ++j)
		{
			// match pointer to src
			if(info->objs.data[j] == src)
			{
				// match type first
				if(info->type == type)
				{
					/* get source component (no need to check
					   if it's NULL or not since the if-blocks
					   above guarantee it won't be NULL)
					*/
					const void *const src_comp = info->components.data[j];

					// when pointer and type match, add only that component to the target
					void *target_comp = pluto_cs_add_component(target, info->type);
					if(!target_comp)
						return 0;

					// run the clone function (if set)
					if(info->clone)
						info->clone(target_comp, src_comp);
					// else just copy raw data
					else
						memcpy(target_comp, src_comp, info->elem_size);

					// if added successfully, return successfully
					vl_log(VL_INFO, "   ^ Cloned component %d and added to obj %p!\n", type, target);
					return 1;
				}
			}
		}
	}

	vl_log(VL_INFO, "Done cloning!\n");

	return 0;
}
int pluto_cs_clone_all_components(const void *const src, void *target)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot clone components, PlutoniumCS was never initialized!\n");
		return 0;
	}
	if(!src)
	{
		vl_log(VL_ERROR, "Cannot clone components from a null source object!\n");
		return 0;
	}
	if(!target)
	{
		vl_log(VL_ERROR, "Cannot add cloned components to a null target!\n");
		return 0;
	}

	vl_log(VL_INFO, "Cloning all components on source object %p:\n", src);

	// find every instance of 'src' and add components to 'target'
	for(size_t i = 0; i < component_types.size; ++i)
	{
		// get info at i
		component_type_info *info = &component_types.data[i];

		// now iterate over obj-component arrays
		for(size_t j = 0; j < info->components.size; ++j)
		{
			// match pointer to src
			if(info->objs.data[j] == src)
			{
				// get source component (guaranteed to be non-null pointer)
				const void *const src_comp = info->components.data[j];

				// when pointer matches, add the same component to the target obj
				void *target_comp = pluto_cs_add_component(target, info->type);
				if(!target_comp)
					return 0;

				// run the clone function (if set)
				if(info->clone)
					info->clone(target_comp, src_comp);
				// else just copy raw data
				else
					memcpy(target_comp, src_comp, info->elem_size);

				vl_log(VL_INFO, "   ^ Cloned component %d and added to obj %p!\n", info->type, target);
			}
		}
	}

	vl_log(VL_INFO, "Done cloning all components!\n");

	return 1;
}
