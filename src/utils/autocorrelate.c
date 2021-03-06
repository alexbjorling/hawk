/* Autocorrelate diffraction pattern. Usage:
   autocorrelate <img.h5>

   Output:
   img_autocorrelation.vtk and img_autocorrelation.png (log scale color jet)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spimage.h>

int main(int argc, char ** argv){
  Image * a;
  Image * b;
  char buffer[1024];
  char base[1024];
  int i;
  real max;
  real min;
  if(argc != 2 && argc != 3){
    printf("Usage: %s <img.h5> [-d]\n"
	   "If -d is given the input is taken to be a diffraction image and the autocorrelation is calculated by taking its fourier transform.\n"
	   "Output: img_autocorrelation.vtk and img_autocorrelation.png\n",argv[0]);
    exit(0);
  }
  a = sp_image_read(argv[1],0);
  for(i = 0;i<sp_c3matrix_size(a->image);i++){
    if(sp_cabs(a->image->data[i]) > 55000){
      a->image->data[i] = sp_cinit(0,0);
    }
  }
  if(argc == 3){
    sp_image_to_intensities(a);
    b = sp_image_fft(a);
    a = sp_image_shift(b);
  }else{
    b = sp_image_cross_correlate(a,a,NULL);
    a = sp_image_shift(b);
  }
  sprintf(base,"%s",argv[1]);
  base[strlen(base)-3] = 0;
  sprintf(buffer,"%s_autocorrelation.vtk",base);
  sp_image_write(a,buffer,0);
  sprintf(buffer,"%s_autocorrelation.png",base);
  sp_image_write(a,buffer,SpColormapJet|SpColormapLogScale);
  max = 0;
  min = 1<< 20;
  for(i = 0;i<sp_c3matrix_size(b->image);i++){
    if(sp_cabs(b->image->data[i]) > max){
      max = sp_cabs(b->image->data[i]);
    }    
    if(sp_cabs(b->image->data[i]) < min){
      min = sp_cabs(b->image->data[i]);
    }    
  }
  /* Cap all values higher than 0.05 than the maximum for easier visualization */
  for(i = 0;i<sp_c3matrix_size(b->image);i++){
    if(sp_cabs(b->image->data[i]) > max*0.15){
      b->image->data[i] = sp_cinit(max*0.15,0);
    }    
  }
  sprintf(buffer,"%s_capped_autocorrelation.png",base);
  a = sp_image_shift(b);
  sp_image_write(a,buffer,SpColormapJet|SpColormapLogScale);  
  return 0;
  
}
