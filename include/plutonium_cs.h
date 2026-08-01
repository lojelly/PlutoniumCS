#pragma once

#ifdef _WIN32
	#ifdef PLUTO_CS_DLL
		#ifdef PLUTO_CS_EXPORTS
			#define PLUTO_CS_API __declspec(dllexport)
		#else
			#define PLUTO_CS_API __declspec(dllimport)
		#endif
	#else
		#define PLUTO_CS_API
	#endif
#else
	#define PLUTO_CS_API
#endif

/**
  Represents the invalid component type.
*/
#define PLUTO_CS_INVALID_COMPONENT -1

/**
  For components, they require an init function
  in the registry so PlutoniumCS knows how to instantiate
  each one when requested. The init function should
  just return an instance of the component.

  @param component The component to initialize.
  @param owner The object pointer that is paired with the
  component; the object that owns this component.
*/
typedef void (*pluto_cs_init_fn) (void *component, void *owner);

/**
  A component's clone function copies the component's data/attributes
  into another component of the same type.

  @param target The component obtaining the cloned data/attributes.
  @param src The original component that was cloned.
*/
typedef void (*pluto_cs_clone_fn) (void *target, const void *const src);

/**
  Initializes the PlutoniumCS library.

  @return 1 on success, 0 on failure.
*/
PLUTO_CS_API int pluto_cs_init(void);
/**
  Shuts down the PlutoniumCS library.
*/
PLUTO_CS_API void pluto_cs_shutdown(void);

/**
  Registers component type info for the component system.

  @important You must call this function for every
  type of component/component extension required in your program.

  @param type The type identifier of the component/component extension to
  register. You should only register each type once. No two
  components/component extensions can have the same type.
  @param size_bytes The size of the component in bytes. For
  example, sizeof(my_component).
  @param init_fn The init function for the component. See
  pluto_cs_init_fn.
  @param clone_fn The clone function for the component. Unlike
  the initialization function, the clone function can be NULL.
  When it is NULL, PlutoniumCS copies the raw data automatically.
  The clone function can be used to add custom cloning functionality if
  desired. See pluto_cs_clone_fn.

  @see pluto_cs_init_fn
  @see pluto_cs_clone_fn
*/
PLUTO_CS_API int pluto_cs_register(int type, size_t size_bytes, pluto_cs_init_fn init_fn, pluto_cs_clone_fn clone_fn);

/**
  Adds a component/component extension to an object.

  @note When calling this function, the 'obj' pointer
  is expected to be handled by the user. Additionally,
  it should be a valid pointer for the entirety of the program.
  PlutoniumCS tracks objects and their components by comparing
  pointers.

  @return A valid component pointer on success, NULL on failure.
*/
PLUTO_CS_API void *pluto_cs_add_component(void *obj, int type);

/**
  Removes a component/component extension from an object.

  @return 1 if the component was successfully removed, 0
  if the removal failed in any way.

  @see pluto_cs_add_component(void*, int)
*/
PLUTO_CS_API int pluto_cs_remove_component(void *obj, int type);

/**
  Returns whether or not an object owns a component
  of the specified type.
*/
PLUTO_CS_API bool pluto_cs_check_component(const void *const obj, int type);

/**
  Returns an object's component of the matching type,
  if it is found, and returns NULL if the component
  isn't found.
*/
PLUTO_CS_API void *pluto_cs_get_component(const void *const obj, int type);

/**
  Obtains the owner of a valid component.

  @important You must only pass in components
  returned by PlutoniumCS functions!
*/
PLUTO_CS_API void *pluto_cs_get_owner(const void *const component);
/**
  Obtains the type of a component.

  @important You must only pass in components
  returned by PlutoniumCS functions!

  @return PLUTO_CS_INVALID_COMPONENT if the function fails,
  a valid component type if it succeeds.
*/
PLUTO_CS_API int pluto_cs_get_type(const void *const component);

/**
  Clones a specific component from one object
  and adds it to another object.

  @param src The source object to clone from.
  @param target The object obtaining the cloned component.
  @param type The component type.

  @return 1 on success, 0 on failure.

  @see pluto_cs_clone_all_components(const void *const, void*)
*/
PLUTO_CS_API int pluto_cs_clone_single_component(const void *const src, void *target, int type);
/**
  Clones components from one object and adds them
  to another object.

  @param src The source object to clone.
  @param target The object to add the cloned components to.

  @return 1 on success, 0 on failure.

  @see pluto_cs_clone_single_component(const void *const, void*, int)
*/
PLUTO_CS_API int pluto_cs_clone_all_components(const void *const src, void *target);
