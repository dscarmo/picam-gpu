#include <stdio.h>
#include <unistd.h>
#include "camera.h"
#include "graphics.h"
#include <time.h>
#include <curses.h>
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <sys/types.h>
#include <dirent.h>
#include <omp.h>



#define PIC_WIDTH 2592
#define PIC_HEIGHT 1944

#define HSIZE 1
#define WSIZE 0.79012345678901234

//#define MAIN_TEXTURE_WIDTH PIC_WIDTH*DSIZE
//#define MAIN_TEXTURE_HEIGHT PIC_HEIGHT*DSIZE


const int MAIN_TEXTURE_WIDTH = PIC_WIDTH*WSIZE;
const int MAIN_TEXTURE_HEIGHT = PIC_HEIGHT*HSIZE;

#define LOAD_ERROR -1

const char *input_dir = "/home/pi/dataset/thousand";

using namespace std;
using namespace cv;


//This will become a class

GfxTexture ytexture, greysobeltexture, mediantexture;
GfxTexture ytexture2, greysobeltexture2, mediantexture2;


void *pic_data;
void *pic_data2;

uchar * pixelData;
uchar * pixelData2;
uchar * zeros;

void * output1;
void * output2;

Mat image;
Mat background;
Mat outputImage, concat1, concat2;
Mat centres;

void imageToGpu();
void gpuBlur();
void gpuEdge();

int gpuInit(){


	//initialize graphics and the camera
	InitGraphics();
	cout << "graphics initialized" << endl;

	ytexture.CreateGreyScale(PIC_WIDTH/2,MAIN_TEXTURE_HEIGHT);
	ytexture2.CreateGreyScale(PIC_WIDTH/2,MAIN_TEXTURE_HEIGHT);
		
	
	greysobeltexture.CreateRGBA(PIC_WIDTH/2, MAIN_TEXTURE_HEIGHT);
	greysobeltexture.GenerateFrameBuffer();

	greysobeltexture2.CreateRGBA(PIC_WIDTH/2, MAIN_TEXTURE_HEIGHT);
	greysobeltexture2.GenerateFrameBuffer();
	

	mediantexture.CreateRGBA(PIC_WIDTH/2, MAIN_TEXTURE_HEIGHT);
	mediantexture.GenerateFrameBuffer();

	mediantexture2.CreateRGBA(PIC_WIDTH/2, MAIN_TEXTURE_HEIGHT);
	mediantexture2.GenerateFrameBuffer();


	background = imread("background.jpg");
	
	if (background.empty()){
		cout << "reference image not found" << endl;
		return LOAD_ERROR; 	
	}
	int imgSize = background.total();
	
	
	//Allocate buffers for pixel Data and U and V fillings
	//Needs "zeros" to fill YUV image
	pixelData = new uchar[imgSize/2];
	pixelData2 = new uchar[imgSize/2];
	zeros = new uchar[imgSize];
	
	for(int i =0; i < background.rows; i++)
		for(int j = 0; j < background.cols; j++){
			zeros[background.cols*i + j] = 125;
		}


	//Fixes bit allignment problems
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	
	//Allocate output pixel data
	output1 = malloc(greysobeltexture.GetWidth()*greysobeltexture.GetHeight()*4);
	output2 = malloc(greysobeltexture.GetWidth()*greysobeltexture.GetHeight()*4);

	return 0;
}

//Input: Saturation channel from HSV image
void imageToGpu(){
	clock_t read_start = clock();
	
	// Pre load pixel data
	printf("starting pixel data filling\n");
	for(int i = 0; i < MAIN_TEXTURE_HEIGHT; i++){
		for(int j = 0; j < PIC_WIDTH; j++){
			if (j < PIC_WIDTH/2)
				pixelData[PIC_WIDTH/2*i + j] = image.at<uchar>(i, j);
			else
				pixelData2[PIC_WIDTH/2*i + (j - PIC_WIDTH/2)] = image.at<uchar>(i, j);	
		}		
	}
	printf("after pixel data\n");		

	pic_data = (void*)pixelData;
	pic_data2 = (void*)pixelData2;
	printf("time passed for reading image: %f\n", (float)(clock() - read_start)/CLOCKS_PER_SEC );


	//Pass data to texture
	const uint8_t* data = (const uint8_t*)pic_data;
	const uint8_t* data2 = (const uint8_t*)pic_data2;
	ytexture.SetPixels(data);
	ytexture2.SetPixels(data2);
}

void gpuBlur()
{
	
	clock_t begin_proc = clock();
	DrawMedianRect(&ytexture,-1.f,-1.f,1.f,1.f,&mediantexture);
	printf("median time: %f\n", (float)(clock() - begin_proc)/CLOCKS_PER_SEC );
	DrawMedianRect(&ytexture2,-1.f,-1.f,1.f,1.f,&mediantexture2);
	printf("median time: %f\n", (float)(clock() - begin_proc)/CLOCKS_PER_SEC );

	clock_t debug_time = clock();
	concat1 = mediantexture.Save("tex_blur.png", "blur", output1);
	concat2 = mediantexture2.Save("tex_blur2.png", "blur", output2);
	hconcat(concat1, concat2, outputImage);		
	if (debug) imwrite("out.jpg", outputImage);	
	printf("concat time: %f\n", (float)(clock() - debug_time)/CLOCKS_PER_SEC );	
}

//Only call after filling mediantexture
void gpuEdge(){
	DrawSobelRect(&mediantexture,-1.f,-1.f,1.f,1.f,&greysobeltexture);
	DrawSobelRect(&mediantexture2,-1.f,-1.f,1.f,1.f,&greysobeltexture2);
	concat1 = greysobeltexture.Save("tex_sobel.png", "sobel", output1);
	concat2 = greysobeltexture2.Save("tex_sobel2.png", "sobel", output2);
	hconcat(concat1, concat2, outputImage);		

}

Mat getGpuOutput(){
	return outputImage;
}

void clean(){
	free(output1);
	free(output2);
	free(pixelData);
	free(pixelData2);
}

int main(){
	gpuInit();
	cvtColor(background, image, CV_BGR2GRAY);
	imageToGpu();
	gpuBlur();
	
	//gpuEdge();
}
