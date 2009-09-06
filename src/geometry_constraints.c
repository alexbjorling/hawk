#include "geometry_constraints.h"
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_blas.h>

geometrically_constrained_system * geometrically_constrained_system_alloc(){
  geometrically_constrained_system * ret = sp_malloc(sizeof(geometrically_constrained_system));
  ret->n_constraints = 0;
  ret->n_variables = 0;
  ret->constraints = 0;
  ret->variables = 0;
  return ret;
}

void geometrically_constrained_system_add_constraint(geometrically_constrained_system * gc, geometric_constraint c){  
  gc->n_constraints++;
  gc->constraints = realloc(gc->constraints,sizeof(geometric_constraint)*gc->n_constraints);
  gc->constraints[gc->n_constraints-1] = c;
}

void geometrically_constrained_system_add_variable(geometrically_constrained_system * gc, geometry_variable v){  
  gc->n_variables++;
  gc->variables = realloc(gc->variables,sizeof(geometry_variable)*gc->n_variables);
  gc->variables[gc->n_variables-1] = v;
}

control_point create_control_point(positioned_image * parent, real x, real y){
  control_point ret;
  ret.parent = parent;
  ret.pos[0] = x;
  ret.pos[1] = y;
  return ret;
}


geometry_variable create_geometry_variable(positioned_image * parent, GeometryVariableType type){
  geometry_variable ret;
  ret.parent = parent;
  ret.type = type;
  return ret;
}
positioned_image * create_positioned_image(const Image * a){
  positioned_image * ret = sp_malloc(sizeof(positioned_image));;
  ret->image = a;
  for(int i = 0;i<NumberOfGeometryVariablesTypes;i++){
    if(i == Zoom){
      ret->pos[i] = 1;
    }else{
      ret->pos[i] = 0;
    }
  }
  return ret;
}

geometric_constraint geometric_constraint_init(GeometryConstraintType type, real best_guess){
  geometric_constraint ret;
  ret.n_points = 0;
  ret.points = 0;
  ret.best_fit = best_guess;
  ret.type = type;
  ret.error = NULL;
  return ret;
}

void geometric_constraint_add_point(geometric_constraint * c,control_point p){
  c->n_points++;
  c->points = realloc(c->points,sizeof(control_point)*c->n_points);
  c->points[c->n_points-1] = p;
}


void set_image_position(positioned_image *  image ,GeometryVariableType t, real v){
  image->pos[t] = v;
}


void geometry_constraints_free(geometry_constraints * gc){
  /* the image themselves are not considered to be own by the constraints so they are not freed */
  sp_free(gc->images);
  for(int i = 0;i<gc->n_images;i++){
    for(int j = 0;j<gc->n_control_points[i];j++){
      sp_vector_free(gc->control_points[i][j]);      
    }
    sp_free(gc->control_points[i]);
  }
  sp_free(gc->control_points);
  sp_free(gc->n_control_points);
  for(int i = 0;i<gc->n_images;i++){
    sp_free(gc->variable_type[i]);
  }
  sp_free(gc->n_variables);
  sp_free(gc->dx);
  sp_free(gc->dy);
  sp_free(gc->zoom);
  sp_free(gc->theta);
  sp_free(gc); 
}

void affine_transform_free(affine_transform * t){
  sp_matrix_free(t->A);
  sp_vector_free(t->b);
  sp_free(t);
}

/* Calculates an affine transform from a gives displacement (dx,dy) plus a scaling zoom and an anti-clockwise angle theta in radians */
affine_transform * affine_transfrom_from_parameters(real dx,real dy,real zoom, real theta){
  affine_transform * ret = sp_malloc(sizeof(affine_transform));
  ret->A = sp_matrix_alloc(2,2);
  ret->b = sp_vector_alloc(2);
  /* Rotation matrix is given by 
     cos  sin 
     -sin cos
  */
  /* Calculate rotation first */
  sp_matrix_set(ret->A,0,0,cos(theta));
  sp_matrix_set(ret->A,0,1,-sin(theta));
  sp_matrix_set(ret->A,1,0,sin(theta));
  sp_matrix_set(ret->A,1,1,cos(theta));
  /* Apply scaling now */
  sp_matrix_scale(ret->A,zoom);
  
  /* Set translation */
  sp_vector_set(ret->b,0,dx);
  sp_vector_set(ret->b,1,dy);
  return ret;
}

/* Calculates an affine transform from a gives displacement (dx,dy) plus a scaling zoom and an anti-clockwise angle theta in radians */
affine_transform * affine_transfrom_from_positioned_image(positioned_image * image){
  return affine_transfrom_from_parameters(image->pos[0],image->pos[1],image->pos[2],image->pos[3]);
}

sp_vector * apply_affine_transform(affine_transform * t, sp_vector * p){
  if(sp_vector_size(p) != 2){
    return NULL;
  }
  if(sp_vector_size(p) != sp_vector_size(t->b)){
    return NULL;
  }
  sp_vector * ret = sp_matrix_vector_prod(t->A,p);
  sp_vector_add(ret,t->b);
  return ret;  
}

affine_transform ** affine_transforms_from_constraints(geometry_constraints * gc){
  affine_transform ** ret = sp_malloc(sizeof(affine_transform *) * gc->n_images);
  for(int i = 0;i<gc->n_images;i++){
    /* the theta parameter is given in degrees but we want it in radians */
    ret[i] = affine_transfrom_from_parameters(gc->dx[i],gc->dy[i],gc->zoom[i],M_PI*gc->theta[i]/180.0);
  }
  return ret;
}

sp_vector *** control_points_to_global(geometry_constraints * gc, affine_transform ** t){
  sp_vector *** ret = sp_malloc(sizeof(sp_vector **)*gc->n_images);
  sp_vector * tmp = sp_vector_alloc(2);
  for(int i = 0;i<gc->n_images;i++){
    ret[i] = sp_malloc(sizeof(sp_vector *)*gc->n_control_points[i]);
    for(int j = 0;j<gc->n_control_points[i];j++){
      sp_vector_set(tmp,0,sp_vector_get(gc->control_points[i][j],0)-sp_image_x(gc->images[i])/2);
      sp_vector_set(tmp,1,sp_image_y(gc->images[i])/2-sp_vector_get(gc->control_points[i][j],1));
      ret[i][j] = apply_affine_transform(t[i],tmp);      
    }
  }
  sp_vector_free(tmp);
  return ret;
}

sp_vector ** control_point_list_to_global(control_point * points, int n){
  sp_vector ** ret = sp_malloc(sizeof(sp_vector *)*n);
  sp_vector * tmp = sp_vector_alloc(2);
  for(int i = 0;i<n;i++){
    affine_transform * t = affine_transfrom_from_positioned_image(points[i].parent);
    sp_vector_set(tmp,0,points[i].pos[0]);
    sp_vector_set(tmp,1,points[i].pos[1]);
    ret[i] = apply_affine_transform(t,tmp);      
    affine_transform_free(t);
  }
  sp_vector_free(tmp);
  return ret;
}


/* projects the point p onto the line through the origin at an angle theta
   Ex: for for theta == 0 that line is y = 0x for theta == pi/4 it's y = x */
sp_vector * project_point_on_line_through_origin(sp_vector * p,real theta){
  if(sp_vector_size(p) != 2){
    return NULL;
  }
  sp_matrix * proj_m = sp_matrix_alloc(2,2);
  sp_vector * line = sp_vector_alloc(2);
  sp_vector_set(line,0,cos(theta));
  sp_vector_set(line,1,sin(theta));
  sp_matrix_set(proj_m,0,0,sp_vector_get(line,0)*sp_vector_get(line,0));
  sp_matrix_set(proj_m,0,1,sp_vector_get(line,0)*sp_vector_get(line,1));
  sp_matrix_set(proj_m,1,0,sp_vector_get(line,0)*sp_vector_get(line,1));
  sp_matrix_set(proj_m,1,1,sp_vector_get(line,1)*sp_vector_get(line,1));
  sp_matrix_scale(proj_m,1.0/(sp_vector_get(line,0)*sp_vector_get(line,0)+sp_vector_get(line,1)*sp_vector_get(line,1)));
  return sp_matrix_vector_prod(proj_m,p);
}





int evaluate_radial_line(const gsl_vector * x, void * params, gsl_vector * f){
  geometrically_constrained_system * gc = (geometrically_constrained_system *)params;
  /* apply the current best parameters to the geometry constraints */  
  for(int i = 0;i<gc->n_variables;i++){
    GeometryVariableType type = gc->variables[i].type;
    gc->variables[i].parent->pos[type] = gsl_vector_get(x,i);
  }
  int index = 0;
  for(int i = 0;i<gc->n_constraints;i++){
    gc->constraints[i].best_fit = gsl_vector_get(x,gc->n_variables+i);
    sp_vector ** cp = control_point_list_to_global(gc->constraints[i].points,gc->constraints[i].n_points);
    for(int j = 0;j<gc->constraints[i].n_points;j++){
      if(gc->constraints[i].type == RadialLineConstraint){
	sp_vector * proj = project_point_on_line_through_origin(cp[j],gc->constraints[i].best_fit);
	sp_vector_sub(proj,cp[j]);
	gsl_vector_set(f,index,sp_vector_norm(proj));
	index++;
	sp_vector_free(proj);
      }
    }
  }
  return GSL_SUCCESS;
}


int evaluate_radial_line_df(const gsl_vector * x, void * params, gsl_matrix * J){
  /* numerically calculate derivatives using central difference */
  gsl_vector * f1 = gsl_vector_alloc(J->size1);
  gsl_vector * f2 = gsl_vector_alloc(J->size1);
  gsl_vector * x1 = gsl_vector_alloc(x->size);
  gsl_vector * x2 = gsl_vector_alloc(x->size);
  gsl_vector_memcpy(x1,x);
  gsl_vector_memcpy(x2,x);
  double delta = 1e-4;
  for(unsigned int i = 0;i<x->size;i++){
    gsl_vector_set(x1,i,gsl_vector_get(x1,i)-delta);
    evaluate_radial_line(x1,params,f1);
    gsl_vector_set(x2,i,gsl_vector_get(x2,i)+delta);
    evaluate_radial_line(x2,params,f2);
    gsl_vector_sub(f2,f1);
    gsl_vector_scale(f2,1.0/(2*delta));
    gsl_matrix_set_col(J,i,f2);
  }
  return GSL_SUCCESS;
}

int evaluate_radial_line_fdf(const gsl_vector * x, void * params,gsl_vector * f, gsl_matrix * J){
  evaluate_radial_line(x,params,f);
  evaluate_radial_line_df(x,params,J);
  return GSL_SUCCESS;
}

void
print_state (int iter, gsl_multifit_fdfsolver * s)
     {
       printf ("iter: %3d\nx = ",iter);
       for(unsigned int i = 0;i<s->x->size;i++){
	 printf("%g ",gsl_vector_get (s->x, i));
       }
       printf("\n|f(x)| = %g\n",gsl_blas_dnrm2 (s->f));
     }

int minimize_geometry_contraint_error(geometrically_constrained_system * gc){
  const gsl_multifit_fdfsolver_type * T = gsl_multifit_fdfsolver_lmder;
  
  int total_points = 0;
  for(int i = 0;i<gc->n_constraints;i++){
    total_points += gc->constraints[i].n_points;
  }
  gsl_multifit_function_fdf my_func;
  my_func.n = total_points;
  /* the variables are the degrees of freedom of the image and the contraints 
     represent the single parameter best fit for each constraint */
  my_func.p = gc->n_variables+gc->n_constraints;
  my_func.f = &evaluate_radial_line;
  my_func.df = &evaluate_radial_line_df;
  my_func.fdf = &evaluate_radial_line_fdf;
  my_func.params = gc;

  gsl_multifit_fdfsolver * s = gsl_multifit_fdfsolver_alloc (T, my_func.n,my_func.p);
  gsl_vector * x = gsl_vector_alloc(my_func.p);
  for(int i = 0;i<gc->n_variables;i++){
    GeometryVariableType type = gc->variables[i].type;
    gsl_vector_set(x,i,gc->variables[i].parent->pos[type]);  
  }
  for(int i = 0;i<gc->n_constraints;i++){
    gsl_vector_set(x,gc->n_variables+i,gc->constraints[i].best_fit);
  }
  gsl_multifit_fdfsolver_set(s,&my_func,x);    
  int iter = 0;
  int status;
  do
    {
      iter++;
      status = gsl_multifit_fdfsolver_iterate (s);     
      printf ("status = %s\n", gsl_strerror (status));     
      print_state (iter, s);      
      if (status)
	break;     
      status = gsl_multifit_test_delta (s->dx, s->x,
					1e-4, 1e-4);
    }
  while (status == GSL_CONTINUE && iter < 500);
  int index = 0;
  /* copy errors */
  for(int i = 0;i<gc->n_constraints;i++){
    gc->constraints[i].error = sp_malloc(sizeof(real)*gc->constraints[i].n_points);
    for(int j = 0;j<gc->constraints[i].n_points;j++){
      gc->constraints[i].error[j] = gsl_vector_get(s->f, index);
      index++;
    }
  }
  return 0;
}

int geometry_contraint_minimizer(geometrically_constrained_system * gc){
  int dummy = gc->n_variables;
  gc->n_variables = 0;
  /* first try to find a decent starting best_fit for all the constraints  */
  minimize_geometry_contraint_error(gc);
  gc->n_variables = dummy;
  /* now run the real thing */
  return minimize_geometry_contraint_error(gc);
}