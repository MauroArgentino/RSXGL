//-*-C++-*-
///////////////////////////////////////////////////////////////////
//
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// gl_object_storage.h - Store objects associated with integral names.
//
// The following classes implement the OpenGL 3.1 object model, where
// objects (such as shading programs) are created and associated 
// with integer names generated by the library. For some object types,
// names are generated separately from creating the objects
// themselves; these classes support that pattern.
//
// Objects are stored contigously in C-style arrays that grow as
// new objects are added. Instances of the classes contained herein
// "own" the objects, handling their destruction when the objects are
// destroyed or when the namespace array itself goes out of scope.
//
// It's expected that some OpenGL object types may have two parts to
// them - a "hot" part that is used frequently in a rendering loop,
// and a "cold" part that is accessed less frequently (for instance,
// when creating, destroying, or querying the object). To potentially
// make better use of the processor's cache, these two parts may be
// "striped" into separate arrays.
//
// Class striped_gl_object_storage implements all of the object creation
// and storage functionality. Class gl_object_storage is a specialization
// that stores entire objects in one array, and class
// cold_hot_gl_object_storage is a specialization that implements the
// "hot and cold" object composition pattern described above.

#ifndef rsxgl_gl_object_storage_H
#define rsxgl_gl_object_storage_H

#if !defined(assert)
#include <cassert>
#endif

#include "array.h"
#include "striped_object_array.h"

#include <memory>
#include <algorithm>
#include <utility>
#include <boost/integer.hpp>
#include <boost/integer/integer_mask.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/fusion/include/mpl.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/transformation.hpp>
#include <boost/type_traits.hpp>

#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>

#include <iostream>

// ObjectsT is a boost::fusion::vector of types:
template< typename ObjectsT,
	  typename NameT = uint32_t,
	  int DefaultObject = 0,
	  typename NameBitfieldT = boost::uintmax_t,
	  size_t ObjectAlign = 128,
	  typename NamesAlloc = std::allocator< NameBitfieldT > >
class striped_gl_object_storage
{
public:

  typedef ObjectsT objects_type;
  typedef NameT name_type;

protected:

  typedef typename NamesAlloc::template rebind< name_type >::other name_allocator;

  typedef NameBitfieldT name_bitfield_type;
  static const size_t name_bitfield_type_bits = std::numeric_limits< name_bitfield_type >::digits;
  static const size_t name_bitfield_type_positions = name_bitfield_type_bits / 2;

  typedef typename NamesAlloc::template rebind< name_bitfield_type >::other name_bitfield_allocator;
  static const name_bitfield_type name_bitfield_mask = boost::low_bits_mask_t< 2 >::sig_bits_fast;
  static const name_bitfield_type name_bitfield_named_mask = boost::high_bit_mask_t< 1 >::bit_position;
  static const name_bitfield_type name_bitfield_init_mask = boost::high_bit_mask_t< 2 >::bit_position;

  typedef array< name_bitfield_type, name_type, NamesAlloc > name_bitfield_array_type;

  name_type m_name_bitfield_size;
  typename name_bitfield_array_type::pointer_type m_name_bitfield;

  typename name_bitfield_array_type::type name_bitfield() {
    return typename name_bitfield_array_type::type(m_name_bitfield,m_name_bitfield_size);
  }

  typename name_bitfield_array_type::const_type name_bitfield() const {
    return typename name_bitfield_array_type::const_type(m_name_bitfield,m_name_bitfield_size);
  }

  typedef boost::circular_buffer< name_type > name_queue_type;
  name_queue_type m_name_queue;

  //
  static inline std::pair< size_t, size_t > 
  name_bitfield_location(const name_type name) {
    const size_t name_position2 = name << 1;
    return std::pair< size_t, size_t >(name_position2 / name_bitfield_type_bits,name_position2 % name_bitfield_type_bits);
  }

  struct created_predicate {
    typename name_bitfield_array_type::const_type m_name_bitfield;

    created_predicate(typename name_bitfield_array_type::type name_bitfield)
      : m_name_bitfield(name_bitfield) {
    }

    created_predicate(typename name_bitfield_array_type::const_type name_bitfield)
      : m_name_bitfield(name_bitfield) {
    }

    bool operator()(name_type name) const {
      size_t name_bitfield_index, name_bitfield_position2;
      boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);
      if((name_bitfield_index < m_name_bitfield.size) && ((m_name_bitfield[name_bitfield_index] & (name_bitfield_init_mask << name_bitfield_position2)) != 0)) {
	return true;
      }
      else {
	return false;
      }
    }
  };

public:

  typedef striped_object_array< ObjectsT, name_type, ObjectAlign > contents_type;
  typename contents_type::pointers_type m_contents, m_orphans;

  typedef typename contents_type::size_type size_type;
  typedef typename contents_type::size_type orphan_size_type;

  size_type m_contents_size;
  orphan_size_type m_orphans_size;

  static const size_type m_contents_grow = 1;
  static const size_type m_orphans_grow = 1;

  // These counts may not actually be needed - the arrays that contain these objects also
  // store a size, although those numbers represent the amount of space allocated for these and
  // not the actual amount used. In the few cases where the library needs to iterate over all
  // of the objects that have been named, created, or orphaned, it can use the full sizes
  // and just check each name's properties.
#if !defined(NDEBUG)
  size_type m_num_names, m_num_objects;
#endif

  // Orphan list grows linearly - so leave this in.
  orphan_size_type m_num_orphans;

  //
  typename contents_type::type contents() {
    return typename contents_type::type(m_contents,m_contents_size);
  }

  typename contents_type::const_type contents() const {
    return typename contents_type::const_type(m_contents,m_contents_size);
  }

  // Number of names that can be accommodated without growing the name array:
  typename contents_type::size_type current_potential_size() const {
    return m_name_bitfield_size * name_bitfield_type_positions;
  }

  // Number of objects that can be accommodated without growing the contents array:
  typename contents_type::size_type contents_size() const {
    return m_contents_size;
  }

  // Number of orphans that can be accommodated without growing the contents array:
  typename contents_type::size_type orphans_size() const {
    return m_orphans_size;
  }

  //
  typename contents_type::type orphans() {
    return typename contents_type::type(m_orphans,m_orphans_size);
  }

  typename contents_type::const_type orphans() const {
    return typename contents_type::const_type(m_orphans,m_orphans_size);
  }

  striped_gl_object_storage(const name_type initial_size = 0,void (*init_default_object)(void *) = 0)
    : m_name_queue(initial_size)
#if !defined(NDEBUG)
    ,m_num_names(0), m_num_objects(0), m_num_orphans(0)
#endif
  {
    
    name_bitfield().construct(1,0);
    //name_queue().construct(std::max((name_type)1,initial_size),0);
    contents().allocate(std::max((name_type)1,initial_size));
    orphans().allocate(std::max((typename contents_type::size_type)1,initial_size));

    // create object name 0:
    name_type name = create_name();
    assert(name == 0);

    if(DefaultObject) {
      create_object(name);
      if(init_default_object != 0) {
	(*init_default_object)(this);
      }
    }
  }

  ~striped_gl_object_storage() {
    name_bitfield().destruct();
    //name_queue().destruct();

    created_predicate p(name_bitfield());
    contents().destruct(p);
    orphans().destruct(p);
  }

  name_type create_name() {
    name_type name = 0;

    // No names to reclaim, create a new one:
    if(m_name_queue.empty() /*m_name_queue_head == m_name_queue_tail*/) {
      assert(name_bitfield().size > 0);

      const size_t name_bitfield_index = name_bitfield().size - 1;
      const name_bitfield_type name_bitfield_value = name_bitfield()[name_bitfield_index];

      size_t name_bitfield_position = 0;

      // If either created or init bits are set, skip it:
      name_bitfield_type mask = (name_bitfield_named_mask | name_bitfield_init_mask) << (name_bitfield_position << 1);

      while(((name_bitfield_value & mask) != 0) && (name_bitfield_position < name_bitfield_type_positions)) {
	++name_bitfield_position;
	mask <<= 2;
      }

      name = (name_bitfield_index * name_bitfield_type_positions) + name_bitfield_position;
    }
    // Reclaim the name from the queue:
    else {
      //name = name_queue()[m_name_queue_head];
      //m_name_queue_head = (m_name_queue_head + 1) % name_queue().size;
      name = m_name_queue.front();
      m_name_queue.pop_front();
    }

    // Expand the bitfield that keeps track of generated & created names:
    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);
    const size_t name_bitfield_size = name_bitfield_index + 1;
    
    if(name_bitfield_size > name_bitfield().size) {
      name_bitfield().resize(name_bitfield_size);
    }

    name_bitfield()[name_bitfield_index] &= ~(name_bitfield_type)(name_bitfield_mask << name_bitfield_position2);
    name_bitfield()[name_bitfield_index] |= (name_bitfield_type)(name_bitfield_named_mask << name_bitfield_position2);

#if !defined(NDEBUG)
    ++m_num_names;
#endif
    return name;
  }

  template< typename OtherNameType >
  size_t create_names(const size_t n,OtherNameType * names) {
    size_t i;
    for(i = 0;i < n;++i,++names) {
      *names = create_name();
    }
    return i;
  }

private:

  void destroy_name(const name_type name,const size_t name_bitfield_index,const size_t name_bitfield_position2) {
    assert(name < current_potential_size());
    assert(name != 0);

    // Destroy the name:
    if((name_bitfield()[name_bitfield_index] & (name_bitfield_named_mask << name_bitfield_position2)) != 0) {
      // reclaim the name:
      // name_queue()[m_name_queue_tail] = name;

      if(m_name_queue.full()) {
	const name_type name_queue_grow = std::max((name_type)(m_name_queue.size() / 2),(name_type)1);
	const name_type name_queue_size = m_name_queue.size() + name_queue_grow;

	name_queue_type new_name_queue(name_queue_size);
	std::copy(m_name_queue.begin(),m_name_queue.end(),std::back_inserter(new_name_queue));
	m_name_queue = new_name_queue;
      }

      m_name_queue.push_back(name);
      
      name_bitfield()[name_bitfield_index] &= ~(name_bitfield_type)(name_bitfield_named_mask << name_bitfield_position2);

#if !defined(NDEBUG)
      --m_num_names;
#endif
    }
  }

public:

  // There are three things that can happen if a GL object is glDelete*()'d, depending upon whether
  // or not a GL object is being used by something else. A GL object can be contained by other GL
  // objects (e.g., buffers in vertex array objects), which means that the container holds onto
  // a name that still expects to point to object storage; the object should be destroyed when
  // other objects stop referring to it (these objects have a reference count embeddedin them).
  // A GL object (actually, other resources, such as GPU memory, that the object acquires) may also
  // have unexecuted GPU operations that depend upon it; such an object should be destroyed when
  // those pending operations have passed.
  //
  // destroy() - This is for objects that are unused by anything else. The object's destructor is
  // called and its name reclaimed immediately.
  //
  // detach() - This is for objects that the client wishes to delete but are being held by other
  // objects (their reference count has not fallen to 0). It simply sets a bit to indicate that
  // the object is no longer a valid GL object (is_object() will return false) but does not
  // destroy the object's storage, or reclaim the object's name (is_name() will return true,
  // and is_constructed() will still return true).
  //
  // orphan() - This is for use in the case that a client wants to destroy an object that holds
  // resources that may still be used by a future GPU operation. The contents of the object are
  // moved to another location, the orphan list. Its destructor is /not/ called at this time,
  // but the object's name and previous storage area are reclaimed (is_object(), is_name(),
  // and is_constructed() all return false). The object may only be accessed through the orphan_at()
  // functions.

  // Destroy an object - delete its name, and call the object's destroy() function.
  void destroy(const name_type name) {
    assert(name < current_potential_size());
    assert(name != 0);

    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);

    if(name_bitfield_index < name_bitfield().size) {

      // Destroy the object:
      if((name_bitfield()[name_bitfield_index] & (name_bitfield_init_mask << name_bitfield_position2)) != 0) {
	contents().destruct_item(name);
#if !defined(NDEBUG)
	--m_num_objects;
#endif
      }

      destroy_name(name,name_bitfield_index,name_bitfield_position2);

      // Clear both create and init bits:
      name_bitfield()[name_bitfield_index] &= ~(name_bitfield_type)(name_bitfield_mask << name_bitfield_position2);
    }
  }

  template< int Named, int Constructed >
  void checked_destroy(const name_type name) {
    assert((Named) ? (is_name(name)) : 1);
    assert((Constructed) ? (is_constructed(name)) : 1);
    destroy(name);
  }

  // Detach an object:
  void detach(const name_type name) {
    assert(name < current_potential_size());
    assert(name != 0);

    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);

    if(name_bitfield_index < name_bitfield().size) {
      // Clear the name bit, leave the init bit intact:
      name_bitfield()[name_bitfield_index] &= ~(name_bitfield_type)(name_bitfield_named_mask << name_bitfield_position2);
#if !defined(NDEBUG)
      --m_num_objects;
#endif
    }
  }

  template< int Named, int Constructed >
  void checked_detach(const name_type name) {
    assert((Named) ? (is_name(name)) : 1);
    assert((Constructed) ? (is_constructed(name)) : 1);
    detach(name);
  }

#if 0
  // Orphan the object - make a copy of the object in the orphans list. Client code
  // will later destroy any GPU resources, like memory, that the orphan still occupies.
  orphan_size_type orphan(const name_type name) {
    assert(name < current_potential_size());

    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);

    assert((name_bitfield_index < name_bitfield().size) && (name_bitfield()[name_bitfield_index] & (name_bitfield_init_mask << name_bitfield_position2)));

    // Make room for another orphan:
    if(m_num_orphans >= orphans().size) {
      orphans().resize(m_num_orphans + m_orphans_grow);
    }

    orphans().construct_item(m_num_orphans);

    return m_num_orphans++;
  }

  // Destroy accumulated orphans:
  void destroy_orphans() {
    for(size_t i = 0,n = m_num_orphans;i < n;++i) {
      boost::fusion::for_each(orphans().values,destroy_item_fn(i));
      orphans().destruct_item(i);
    }
    m_num_orphans = 0;
  }

  void destroy_orphan(const orphan_size_type i) {
    assert(i < m_num_orphans);
    orphans().destruct_item(i);
    --m_num_orphans;
  }
#endif

  void create_object(const name_type name) {
    assert(is_name(name) && !is_constructed(name));

    // Construct the object:
    if(name >= contents().size) {
      contents().resize(name + m_contents_grow);
    }

    contents().construct_item(name);
      
    // Set the created bit:
    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);
    
    assert(name_bitfield_index < name_bitfield().size);
    name_bitfield()[name_bitfield_index] |= (name_bitfield_type)(name_bitfield_init_mask << name_bitfield_position2);

#if !defined(NDEBUG)
    ++m_num_objects;
#endif
  }

  name_type create_name_and_object() {
    name_type name = create_name();
    create_object(name);
    return name;
  }

  // Returns true if name has been allocated (but may not yet be an actual object).
  bool is_name(const name_type name) const {
    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);
    
    if(name_bitfield_index < name_bitfield().size) {
      return (name_bitfield()[name_bitfield_index] & (name_bitfield_named_mask << name_bitfield_position2)) != 0;
    }
    else {
      return false;
    }
  }

  // Returns true if the name has been allocated and its been constructed. For use by
  // functions like glIs*() which test to see if a GL object is fully usable.
  bool is_object(const name_type name) const {
    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);

    if(name_bitfield_index < name_bitfield().size) {
      return ((name_bitfield()[name_bitfield_index] >> name_bitfield_position2) & name_bitfield_mask) == (name_bitfield_named_mask | name_bitfield_init_mask);
    }
    else {
      return false;
    }
  }

  // For use by functions that still want to access an object's storage by name, even if it's
  // been glDelete*()'d and should no longer be accessed by other gl*() functions.
  bool is_constructed(const name_type name) const {
    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);

    if(name_bitfield_index < name_bitfield().size) {
      return (name_bitfield()[name_bitfield_index] & (name_bitfield_init_mask << name_bitfield_position2)) != 0;
    }
    else {
      return false;
    }
  }

  template< size_t I >
  typename boost::add_reference< typename boost::fusion::result_of::at_c< objects_type, I >::type >::type
  at(const name_type i) {
    assert(is_constructed(i));
    assert((DefaultObject == 0) ? (i != 0) : true);
    return contents().template at< I >(i);
  }

  template< size_t I >
  typename boost::add_reference< typename boost::add_const< typename boost::fusion::result_of::at_c< objects_type, I >::type >::type >::type
  at(const name_type i) const {
    assert(is_constructed(i));
    assert((DefaultObject == 0) ? (i != 0) : true);
    return contents().template at< I >(i);
  }

#if 0
  orphan_size_type num_orphans() const {
    return m_num_orphans;
  }

  // Access an orphan by its index. Note that the argument to these functions is /not/ a name - it's an
  // index onto the orphan list.
  template< size_t I >
  typename boost::add_reference< typename boost::fusion::result_of::at_c< objects_type, I >::type >::type
  orphan_at(const orphan_size_type i) {
    assert(i < m_num_orphans);
    return orphans().template at< I >(i);
  }

  template< size_t I >
  typename boost::add_reference< typename boost::add_const< typename boost::fusion::result_of::at_c< objects_type, I >::type >::type >::type
  orphan_at(const orphan_size_type i) const {
    assert(i < m_num_orphans);
    return orphans().template at< I >(i);
  }
#endif
};

template< typename ObjectT,
	  typename NameT = uint32_t,
	  int DefaultObject = 0,
	  typename NameBitfieldT = boost::uintmax_t,

	  size_t ObjectAlign = 128,
	  typename NamesAlloc = std::allocator< NameBitfieldT > >
class gl_object_storage : public striped_gl_object_storage< boost::fusion::vector< ObjectT >, NameT, DefaultObject >
{
public:

  typedef striped_gl_object_storage< boost::fusion::vector< ObjectT >, 
				     NameT, DefaultObject > base_type;

  gl_object_storage(const typename base_type::name_type initial_size = 0,void (*init_default_object)(void *) = 0)
    : base_type(initial_size,init_default_object) {
  }

  ObjectT & at(const typename base_type::name_type i) {
    return base_type::template at<0>(i);
  }

  const ObjectT & at(const typename base_type::name_type i) const {
    return base_type::template at<0>(i);
  }
};

#if 0
// This is presently unused by the library. It's meant for objects that can be divided into
// two parts - a "hot" part that's used by critical sections of the program (e.g., the rendering
// loop) and a "cold" part that's used less frequently (e.g., to support OpenGL's ability to 
// query objects, which a program may not do at all). The storage for each object is similarly
// divided into two arrays, and, in an attempt to promote memory locality, the hot parts are
// kept together away from the cold parts (just like a McDLT).

template< typename ColdT, typename HotT >
struct cold_hot_gl_object_destroy {
  void operator()(ColdT & object) const {
    object.destroy();
  }

  void operator()(HotT & object) const {
    object.destroy();
  }
};

template< typename ColdT, typename HotT,
	  typename NameT = uint32_t,
	  int DefaultObject = 0,
	  typename NameBitfieldT = boost::uintmax_t,
	  size_t ObjectAlign = 128,
	  typename NamesAlloc = std::allocator< NameBitfieldT > >
class cold_hot_gl_object_storage : public striped_gl_object_storage< boost::fusion::vector< ColdT, HotT >, NameT, DefaultObject >
{
public:

  typedef striped_gl_object_storage< boost::fusion::vector< ColdT, HotT >, NameT, DefaultObject > base_type;

  cold_hot_gl_object_storage(const typename base_type::name_type initial_size = 0,void (*init_default_object)(void *) = 0)
    : base_type(initial_size,init_default_object) {
  }

  ColdT & cold(const typename base_type::name_type i) {
    return base_type::template at< 0 >(i);
  }

  const ColdT & cold(const typename base_type::name_type i) const {
    return base_type::template at< 0 >(i);
  }

  HotT & hot(const typename base_type::name_type i) {
    return base_type::template at< 1 >(i);
  }

  const HotT & hot(const typename base_type::name_type i) const {
    return base_type::template at< 1 >(i);
  }
};
#endif

#endif