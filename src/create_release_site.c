/***********************************************************************************
 *                                                                                 *
 * Copyright (C) 2006-2013 by                                                      *
 * The Salk Institute for Biological Studies and                                   *
 * Pittsburgh Supercomputing Center, Carnegie Mellon University                    *
 *                                                                                 *
 * This program is free software; you can redistribute it and/or                   *
 * modify it under the terms of the GNU General Public License                     *
 * as published by the Free Software Foundation; either version 2                  *
 * of the License, or (at your option) any later version.                          *
 *                                                                                 *
 * This program is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                  *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                   *
 * GNU General Public License for more details.                                    *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License               *
 * along with this program; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. *
 *                                                                                 *
 ***********************************************************************************/

#include "create_release_site.h"
#include "create_species.h"
#include "create_object.h"
#include "logging.h"
#include "sym_table.h"

#include <stdlib.h>
#include <string.h>



/**************************************************************************
 new_release_site:
    Create a new release site.

 In: state: system state
     name: name for the new site
 Out: an empty release site, or NULL if allocation failed
**************************************************************************/
struct release_site_obj *
new_release_site(MCELL_STATE *state, char *name)
{
  struct release_site_obj *rel_site_obj_ptr;
  if ((rel_site_obj_ptr = CHECKED_MALLOC_STRUCT(struct release_site_obj, "release site")) == NULL)
    return NULL;
  rel_site_obj_ptr->location = NULL;
  rel_site_obj_ptr->mol_type = NULL;
  rel_site_obj_ptr->release_number_method = CONSTNUM;
  rel_site_obj_ptr->release_shape = SHAPE_UNDEFINED;
  rel_site_obj_ptr->orientation = 0;
  rel_site_obj_ptr->release_number = 0;
  rel_site_obj_ptr->mean_diameter = 0;
  rel_site_obj_ptr->concentration = 0;
  rel_site_obj_ptr->standard_deviation = 0;
  rel_site_obj_ptr->diameter = NULL;
  rel_site_obj_ptr->region_data = NULL;
  rel_site_obj_ptr->mol_list = NULL;
  rel_site_obj_ptr->release_prob = 1.0;
  rel_site_obj_ptr->pattern = state->default_release_pattern;
  //if ((rel_site_obj_ptr->name = mdl_strdup(name)) == NULL)
  if ((rel_site_obj_ptr->name = strdup(name)) == NULL)
  {
    free(rel_site_obj_ptr);
    return NULL;
  }
  return rel_site_obj_ptr;
}



/**************************************************************************
 set_release_site_location:
    Set the location of a release site.

 In: state: system state
     rel_site_obj_ptr: release site
     location: location for release site
 Out: none
**************************************************************************/
void
set_release_site_location(MCELL_STATE *state,
                          struct release_site_obj *rel_site_obj_ptr,
                          struct vector3 *location)
{
  rel_site_obj_ptr->location = location;
  rel_site_obj_ptr->location->x *= state->r_length_unit;
  rel_site_obj_ptr->location->y *= state->r_length_unit;
  rel_site_obj_ptr->location->z *= state->r_length_unit;
}



/**************************************************************************
 start_release_site:
    Start parsing the innards of a release site.

 In: state: system state
     sym_ptr: symbol for the release site
 Out: 0 on success, 1 on failure
**************************************************************************/
struct object *
start_release_site(MCELL_STATE *state,
                   struct sym_table *sym_ptr)
{
  struct object *obj_ptr = (struct object *) sym_ptr->value;
  obj_ptr->object_type = REL_SITE_OBJ;
  obj_ptr->contents = new_release_site(state, sym_ptr->name);
  if (obj_ptr->contents == NULL) {
    return NULL;
  }

  return obj_ptr;
}



/**************************************************************************
 finish_release_site:
    Finish parsing the innards of a release site.

 In: sym_ptr: symbol for the release site
 Out: the object, on success, or NULL on failure
**************************************************************************/
struct object *
finish_release_site(struct sym_table *sym_ptr)
{
  struct object *obj_ptr_new = (struct object *) sym_ptr->value;
  no_printf("Release site %s defined:\n", sym_ptr->name);
  if (is_release_site_valid((struct release_site_obj *) obj_ptr_new->contents)) {
    return NULL;
  }
  return obj_ptr_new;
}



/**************************************************************************
 is_release_site_valid:
    Validate a release site.

 In: rel_site_obj_ptr: the release site object to validate
 Out: 0 if it is valid, 1 if not
**************************************************************************/
int
is_release_site_valid(struct release_site_obj *rel_site_obj_ptr)
{
  // Unless it's a list release, user must specify MOL type 
  if (rel_site_obj_ptr->release_shape != SHAPE_LIST)
  {
    // Must specify molecule to release using MOLECULE=molecule_name.
    if (rel_site_obj_ptr->mol_type == NULL) {
      return 2;
    }

    // Make sure it's not a surface class 
    if ((rel_site_obj_ptr->mol_type->flags & IS_SURFACE) != 0) {
      return 3;
    }
  }

  /* Check that concentration/density status of release site agrees with
   * volume/grid status of molecule */
  if (rel_site_obj_ptr->release_number_method == CCNNUM)
  {
    // CONCENTRATION may only be used with molecules that can diffuse in 3D.
    if ((rel_site_obj_ptr->mol_type->flags & NOT_FREE) != 0) {
      return 4;
    }
  }
  else if (rel_site_obj_ptr->release_number_method == DENSITYNUM)
  {
    // DENSITY may only be used with molecules that can diffuse in 2D.
    if ((rel_site_obj_ptr->mol_type->flags & NOT_FREE) == 0) {
      return 5;
    }
  }

  /* Unless it's a region release we must have a location */
  if (rel_site_obj_ptr->release_shape != SHAPE_REGION)
  {
    if (rel_site_obj_ptr->location == NULL)
    {
      // Release site is missing location.
      if (rel_site_obj_ptr->release_shape!=SHAPE_LIST || rel_site_obj_ptr->mol_list==NULL) {
        return 6;
      }
      else
      {
        // Give it a default location of (0, 0, 0)
        rel_site_obj_ptr->location = CHECKED_MALLOC_STRUCT(struct vector3, "release site location");
        if (rel_site_obj_ptr->location==NULL)
          return 1;
        rel_site_obj_ptr->location->x = 0;
        rel_site_obj_ptr->location->y = 0;
        rel_site_obj_ptr->location->z = 0;
      }
    }
    no_printf(
      "\tLocation = [%f,%f,%f]\n",
      rel_site_obj_ptr->location->x,
      rel_site_obj_ptr->location->y,
      rel_site_obj_ptr->location->z);
  }
  return 0;
}



/**************************************************************************
 set_release_site_geometry_region:
    Set the geometry for a particular release site to be a region expression.

 In: state: system state
     rel_site_obj_ptr: the release site object to validate
     obj_ptr: the object representing this release site
     rel_eval: the release evaluator representing the region of release
 Out: 0 on success, 1 on failure
**************************************************************************/
int
set_release_site_geometry_region(MCELL_STATE *state,
                                 struct release_site_obj *rel_site_obj_ptr,
                                 struct object *obj_ptr,
                                 struct release_evaluator *rel_eval)
{

  rel_site_obj_ptr->release_shape = SHAPE_REGION;
  state->place_waypoints_flag = 1;

  struct release_region_data *rel_reg_data = CHECKED_MALLOC_STRUCT(
    struct release_region_data,
    "release site on region");
  if (rel_reg_data == NULL) {
    return 1;
  }

  rel_reg_data->n_walls_included = -1; /* Indicates uninitialized state */
  rel_reg_data->cum_area_list = NULL;
  rel_reg_data->wall_index = NULL;
  rel_reg_data->obj_index = NULL;
  rel_reg_data->n_objects = -1;
  rel_reg_data->owners = NULL;
  rel_reg_data->in_release = NULL;
  rel_reg_data->self = obj_ptr;

  rel_reg_data->expression = rel_eval;

  if (check_release_regions(rel_eval, obj_ptr, state->root_instance))
  {
    // Trying to release on a region that the release site cannot see! Try
    // grouping the release site and the corresponding geometry with an OBJECT.
    free(rel_reg_data);
    return 2;
  }

  rel_site_obj_ptr->region_data = rel_reg_data;
  return 0;
}



/**************************************************************************
 set_release_site_geometry_object:
    Set the geometry for a particular release site to be an entire object.

 In: rel_site_obj_ptr: the release site object to validate
     obj_ptr: the object upon which to release
 Out: 0 on success, 1 on failure
 NOTE: This is similar to mdl_set_release_site_geometry_object in the parser
**************************************************************************/
int
set_release_site_geometry_object(MCELL_STATE *state,
                                 struct release_site_obj *rel_site_obj_ptr,
                                 struct object *obj_ptr)
{
  if ((obj_ptr->object_type == META_OBJ) ||
      (obj_ptr->object_type == REL_SITE_OBJ)) {
    return 1;
  }

  char *obj_name = obj_ptr->sym->name;
  char *region_name = CHECKED_SPRINTF("%s,ALL", obj_name);
  if (region_name == NULL) {
    return 1;
  }

  struct sym_table *sym_ptr;
  if ((sym_ptr = retrieve_sym(region_name, state->reg_sym_table)) == NULL)
  {
    free(region_name);
    return 1;
  }
  free(region_name);

  struct release_evaluator *rel_eval = CHECKED_MALLOC_STRUCT(
    struct release_evaluator, "release site on region");
  if (rel_eval==NULL) {
    return 1;
  }

  rel_eval->op = REXP_NO_OP | REXP_LEFT_REGION;
  rel_eval->left = sym_ptr->value;
  rel_eval->right = NULL;

  ((struct region*)rel_eval->left)->flags |= COUNT_CONTENTS;

  rel_site_obj_ptr->release_shape = SHAPE_REGION;
  state->place_waypoints_flag = 1;

  if (check_release_regions(rel_eval, obj_ptr, state->root_instance)) {
    return 1;
  }

  struct release_region_data *rel_reg_data = CHECKED_MALLOC_STRUCT(
    struct release_region_data, "release site on region");
  if (rel_reg_data==NULL)
  {
    free(rel_eval);
    return 1;
  }

  rel_reg_data->n_walls_included = -1; /* Indicates uninitialized state */
  rel_reg_data->cum_area_list = NULL;
  rel_reg_data->wall_index = NULL;
  rel_reg_data->obj_index = NULL;
  rel_reg_data->n_objects = -1;
  rel_reg_data->owners = NULL;
  rel_reg_data->in_release = NULL;
  //rel_reg_data->self = parse_state->current_object;
  rel_reg_data->expression = rel_eval;
  rel_site_obj_ptr->region_data = rel_reg_data;

  return 0;
}



/*************************************************************************
 pack_release_expr:

 In: rel_eval_L:  release evaluation tree (set operations) for left side of expression
     rel_eval_R:  release evaluation tree for right side of expression
     op:   flags indicating the operation performed by this node
 Out: release evaluation tree containing the two subtrees and the
      operation
 Note: singleton elements (with REXP_NO_OP operation) are compacted by
       this function and held simply as the corresponding region, not
       the NO_OP operation of that region (the operation is needed for
       efficient parsing)
*************************************************************************/
static struct release_evaluator*
pack_release_expr(struct release_evaluator *rel_eval_L,
                  struct release_evaluator *rel_eval_R,
                  byte op)
{

  struct release_evaluator *rel_eval = NULL;

  if (!(op & REXP_INCLUSION) &&
      (rel_eval_R->op & REXP_MASK) == REXP_NO_OP &&
      (rel_eval_R->op & REXP_LEFT_REGION) != 0)
  {
    if ((rel_eval_L->op & REXP_MASK) == REXP_NO_OP && (rel_eval_L->op & REXP_LEFT_REGION) != 0)
    {
      rel_eval = rel_eval_L;
      rel_eval->right = rel_eval_R->left;
      rel_eval->op = op | REXP_LEFT_REGION | REXP_RIGHT_REGION;
      free(rel_eval_R);
    }
    else
    {
      rel_eval = rel_eval_R;
      rel_eval->right = rel_eval->left;
      rel_eval->left = (void*) rel_eval_L;
      rel_eval->op = op | REXP_RIGHT_REGION;
    }
  }
  else if (!(op&REXP_INCLUSION) && 
           (rel_eval_L->op & REXP_MASK) == REXP_NO_OP && 
           (rel_eval_L->op & REXP_LEFT_REGION) != 0)
  {
    rel_eval = rel_eval_L;
    rel_eval->right = (void*) rel_eval_R;
    rel_eval->op = op | REXP_LEFT_REGION;
  }
  else
  {
    rel_eval = CHECKED_MALLOC_STRUCT(
      struct release_evaluator, "release region expression");
    if (rel_eval == NULL) {
      return NULL;
    }

    rel_eval->left = (void*) rel_eval_L;
    rel_eval->right = (void*) rel_eval_R;
    rel_eval->op = op;
  }

  return rel_eval;
}



/**************************************************************************
 mdl_new_release_region_expr_term:
    Create a new "release on region" expression term.

 In: my_sym: the symbol for the region comprising this term in the expression
 Out: the release evaluator on success, or NULL if allocation fails
**************************************************************************/
struct release_evaluator *
new_release_region_expr_term(struct sym_table *my_sym)
{

  struct release_evaluator *rel_eval = CHECKED_MALLOC_STRUCT(
    struct release_evaluator, "release site on region");
  if (rel_eval == NULL) {
    return NULL;
  }

  rel_eval->op = REXP_NO_OP | REXP_LEFT_REGION;
  rel_eval->left = my_sym->value;
  rel_eval->right = NULL;

  ((struct region*)rel_eval->left)->flags |= COUNT_CONTENTS;
  return rel_eval;
}



/**************************************************************************
 new_release_region_expr_binary:
    Set the geometry for a particular release site to be a region expression.

 In: parse_state: parser state
     reL:  release evaluation tree (set operations) for left side of expression
     reR:  release evaluation tree for right side of expression
     op:   flags indicating the operation performed by this node
 Out: the release expression, or NULL if an error occurs
**************************************************************************/
struct release_evaluator *
new_release_region_expr_binary(struct release_evaluator *rel_eval_L,
                               struct release_evaluator *rel_eval_R,
                               int op)
{
  return pack_release_expr(rel_eval_L, rel_eval_R, op);
}



/**************************************************************************
 check_valid_molecule_release:
    Check that a particular molecule type is valid for inclusion in a release
    site.  Checks that orientations are present if required, and absent if
    forbidden, and that we aren't trying to release a surface class.

 In: state: system state
     mol_type: molecule species and (optional) orientation for release
 Out: 0 on success, 1 on failure
 NOTE: This is similar to mdl_check_valid_molecule_release in the parser
**************************************************************************/
int
check_valid_molecule_release(MCELL_STATE *state,
                             struct species_opt_orient *mol_type)
{

  struct species *mol = (struct species *) mol_type->mol_type->value;
  if (mol->flags & ON_GRID)
  {
    if (!mol_type->orient_set)
    {
      if (state->notify->missed_surf_orient==WARN_ERROR) {
        return 1;
      }
    }
  }
  else if ((mol->flags & NOT_FREE) == 0)
  {
    if (mol_type->orient_set)
    {
      if (state->notify->useless_vol_orient==WARN_ERROR) {
        return 1;
      }
    }
  }
  else {
    return 1;
  }

  return 0;
}



/*************************************************************************
 check_release_regions:

 In: state:    system state
     rel_eval: an release evaluator (set operations applied to regions)
     parent:   the object that owns this release evaluator
     instance: the root object that begins the instance tree
 Out: 0 if all regions refer to instanced objects or to a common ancestor of
      the object with the evaluator, meaning that the object can be found. 1 if
      any referred-to region cannot be found.
*************************************************************************/
int
check_release_regions(struct release_evaluator *rel_eval,
                      struct object *parent,
                      struct object *instance)
{
  struct object *obj_ptr;

  if (rel_eval->left != NULL)
  {
    if (rel_eval->op & REXP_LEFT_REGION)
    {
      obj_ptr = common_ancestor(parent, ((struct region*) rel_eval->left)->parent);
      if (obj_ptr == NULL || (obj_ptr->parent == NULL && obj_ptr!=instance)) {
        obj_ptr = common_ancestor(instance, ((struct region*) rel_eval->left)->parent);
      }

      if (obj_ptr == NULL)
      {
        // Region neither instanced nor grouped with release site
        return 2;
      }
    }
    else if (check_release_regions(rel_eval->left, parent, instance)) {
      return 1;
    }
  }

  if (rel_eval->right != NULL)
  {
    if (rel_eval->op & REXP_RIGHT_REGION)
    {
      obj_ptr = common_ancestor(parent, ((struct region*)rel_eval->right)->parent);
      if (obj_ptr == NULL || (obj_ptr->parent == NULL && obj_ptr != instance)) {
        obj_ptr = common_ancestor(instance, ((struct region*)rel_eval->right)->parent);
      }

      if (obj_ptr == NULL)
      {
        // Region not grouped with release site.
        return 3;
      }
    }
    else if (check_release_regions(rel_eval->right, parent, instance)) {
      return 1;
    }
  }

  return 0;
}



/**************************************************************************
 set_release_site_constant_number:
    Set a constant release quantity from this release site, in units of
    molecules.

 In: rel_site_obj_ptr: the release site
     num:  count of molecules to release
 Out: none.  release site object is updated
**************************************************************************/
void 
set_release_site_constant_number(struct release_site_obj *rel_site_obj_ptr,
                                 double num)
{
  rel_site_obj_ptr->release_number_method = CONSTNUM;
  rel_site_obj_ptr->release_number = num;
}



/**************************************************************************
 set_release_site_gaussian_number:
    Set a gaussian-distributed release quantity from this release site, in
    units of molecules.

 In: rel_site_obj_ptr: the release site
     mean: mean value of distribution
     stdev: std. dev. of distribution
 Out: none.  release site object is updated
**************************************************************************/
void 
set_release_site_gaussian_number(struct release_site_obj *rel_site_obj_ptr,
                                 double mean,
                                 double stdev)
{
  rel_site_obj_ptr->release_number_method = GAUSSNUM;
  rel_site_obj_ptr->release_number = mean;
  rel_site_obj_ptr->standard_deviation = stdev;
}



/**************************************************************************
 set_release_site_volume_dependent_number:
    Set a release quantity from this release site based on a fixed
    concentration in a sphere of a gaussian-distributed diameter with a
    particular mean and std. deviation.

 In: rel_site_obj_ptr: the release site
     mean: mean value of distribution of diameters
     stdev: std. dev. of distribution of diameters
     conc: concentration for release
 Out: none.  release site object is updated
**************************************************************************/
void 
set_release_site_volume_dependent_number(struct release_site_obj *rel_site_obj_ptr,
                                         double mean,
                                         double stdev,
                                         double conc)
{
  rel_site_obj_ptr->release_number_method = VOLNUM;
  rel_site_obj_ptr->mean_diameter = mean;
  rel_site_obj_ptr->standard_deviation = stdev;
  rel_site_obj_ptr->concentration = conc;
}



/**************************************************************************
 release_single_molecule_singleton:
    Populates a list with a single LIST release molecule descriptor.

 In: list: the list
     mol:  the descriptor
 Out: none.  list is updated
**************************************************************************/
void
release_single_molecule_singleton(struct release_single_molecule_list *list,
                                  struct release_single_molecule *mol)
{
  list->rsm_tail = list->rsm_head = mol;
  list->rsm_count = 1;
}



/**************************************************************************
 add_release_single_molecule_to_list:
    Adds a release molecule descriptor to a list.

 In: list: the list
     mol:  the descriptor
 Out: none.  list is updated
**************************************************************************/
void
add_release_single_molecule_to_list(struct release_single_molecule_list *list,
                                    struct release_single_molecule *mol)
{
  list->rsm_tail = list->rsm_tail->next = mol;
  ++ list->rsm_count;
}



/**************************************************************************
 set_release_site_concentration:
    Set a release quantity from this release site based on a fixed
    concentration within the release-site's area.

 In: rel_site_obj_ptr: the release site
     conc: concentration for release
 Out: 0 on success, 1 on failure.  release site object is updated
**************************************************************************/
int 
set_release_site_concentration(struct release_site_obj *rel_site_obj_ptr,
                               double conc)
{
  if (rel_site_obj_ptr->release_shape == SHAPE_SPHERICAL_SHELL) {
    return 1;
  }
  rel_site_obj_ptr->release_number_method = CCNNUM;
  rel_site_obj_ptr->concentration = conc;
  return 0;
}



/**************************************************************************
 set_release_site_density:
    Set a release quantity from this release site based on a fixed
    density within the release-site's area.  (Hopefully we're talking about a
    surface release here.)

 In: rel_site_obj_ptr: the release site
     dens: density for release
 Out: 0 on success, 1 on failure.  release site object is updated
**************************************************************************/
int 
set_release_site_density(struct release_site_obj *rel_site_obj_ptr,
                         double dens)
{

  rel_site_obj_ptr->release_number_method = DENSITYNUM;
  rel_site_obj_ptr->concentration = dens;
  return 0;
}