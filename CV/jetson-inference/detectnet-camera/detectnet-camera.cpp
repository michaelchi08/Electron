/*
 * http://github.com/dusty-nv/jetson-inference
 */

 #include <iostream>
 #include <fstream>

#include "gstCamera.h"

#include "glDisplay.h"
#include "glTexture.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include <cmath>

#include "cudaMappedMemory.h"
#include "cudaNormalize.h"
#include "cudaFont.h"

#include "detectNet.h"


#define DEFAULT_CAMERA 1	// -1 for onboard camera, or change to index of /dev/video V4L2 camera (>=0)


bool signal_recieved = false;

void sig_handler(int signo)
{
	if( signo == SIGINT )
	{
		signal_recieved = true;
	}
}


int main( )
{
	/*
	 * parse network type from CLI arguments
	 */
	/*detectNet::NetworkType networkType = detectNet::PEDNET_MULTI;

	if( argc > 1 )
	{
		if( strcmp(argv[1], "multiped") == 0 || strcmp(argv[1], "pednet") == 0 || strcmp(argv[1], "multiped-500") == 0 )
			networkType = detectNet::PEDNET_MULTI;
		else if( strcmp(argv[1], "ped-100") == 0 )
			networkType = detectNet::PEDNET;
		else if( strcmp(argv[1], "facenet") == 0 || strcmp(argv[1], "facenet-120") == 0 || strcmp(argv[1], "face-120") == 0 )
			networkType = detectNet::FACENET;
	}*/



	/*
	 * create the camera device
	 */
	gstCamera* camera = gstCamera::Create(DEFAULT_CAMERA);

	if( !camera )
	{
		return 0;
	}


	/*
	 * create detectNet
	 */
	detectNet* net = detectNet::Create("networks/InitialLegNet/deploy.prototxt", "networks/InitialLegNet/snapshot_iter_3250.caffemodel", "networks/InitialLegNet/mean.binaryproto", 0.5, "data", "coverage", "bboxes", 2 );

	if( !net )
	{
		return 0;
	}


	/*
	 * allocate memory for output bounding bounistdxes and class confidence
	 */
	const uint32_t maxBoxes = net->GetMaxBoundingBoxes();
	const uint32_t classes  = net->GetNumClasses();

	float* bbCPU    = NULL;
	float* bbCUDA   = NULL;
	float* confCPU  = NULL;
	float* confCUDA = NULL;

	if( !cudaAllocMapped((void**)&bbCPU, (void**)&bbCUDA, maxBoxes * sizeof(float4)) ||
	    !cudaAllocMapped((void**)&confCPU, (void**)&confCUDA, maxBoxes * classes * sizeof(float)) )
	{
		return 0;
	}


	/*
	 * create openGL window
	 */
	glDisplay* display = glDisplay::Create();
	glTexture* texture = NULL;

	if( !display ) {

	}
	else
	{
		texture = glTexture::Create(camera->GetWidth(), camera->GetHeight(), GL_RGBA32F_ARB/*GL_RGBA8*/);
	}
unistd

	/*
	 * create font
	 */
	cudaFont* font = cudaFont::Create();


	/*
	 * start streaming
	 */
	if( !camera->Open() )
	{
		return 0;
	}


	/*
	 * processing loop
	 */
	float confidence = 0.0f;
	int count = 0;
	std::ofstream myfile;

	while( !signal_recieved )
	{
		void* imgCPU  = NULL;
		void* imgCUDA = NULL;

		// get the latest frame
		camera->Capture(&imgCPU, &imgCUDA, 1000);

		// convert from YUV to RGBA
		void* imgRGBA = NULL;

		camera->ConvertRGBA(imgCUDA, &imgRGBA);

		// classify image with detectNet
		int numBoundingBoxes = maxBoxes;

		if( net->Detect((float*)imgRGBA, camera->GetWidth(), camera->GetHeight(), bbCPU, &numBoundingBoxes, confCPU))
		{

			int lastClass = 0;
			int lastStart = 0;

			for( int n=0; n < numBoundingBoxes; n++ )
			{
				const int nc = confCPU[n*2+1];
				float* bb = bbCPU + (n * 4);

        float visionAngle = 1.36136;
				if (count % 2 == 0) {
					printf("%f %f %f %f\n", bb[0], bb[1], bb[2], bb[3]);
          float width = camera->GetWidth();
          float height = camera->GetHeight();
          float baseLength = sqrt(pow((bb[0]-(width/2)), 2) + pow((bb[1]-(height/2)), 2))
          float baseHeight = baseLength/tan(visionAngle/2)
          printf("left: %f", atan(triangle_base_length / triangle_base_height));
          width = camera->GetWidth();
          height = camera->GetHeight();
          baseLength = sqrt(pow((bb[2]-(width/2)), 2) + pow((bb[1]-(height/2)), 2))
          baseHeight = baseLength/tan(visionAngle/2)
          printf("right: %f\n", atan(triangle_base_length / triangle_base_height));
          printf()
				}
				count+=1;


				if( nc != lastClass || n == (numBoundingBoxes - 1) )
				{
					net->DrawBoxes((float*)imgRGBA, (float*)imgRGBA, camera->GetWidth(), camera->GetHeight(),
						                        bbCUDA + (lastStart * 4), (n - lastStart) + 1, lastClass);

					lastClass = nc;
					lastStart = n;

					CUDA(cudaDeviceSynchronize());
				}
			}

			/*if( font != NULL )
			{
				char str[256];
				sprintf(str, "%05.2f%% %s", confidence * 100.0f, net->GetClassDesc(img_class));

				font->RenderOverlay((float4*)imgRGBA, (float4*)imgRGBA, camera->GetWidth(), camera->GetHeight(),
								    str, 10, 10, make_float4(255.0f, 255.0f, 255.0f, 255.0f));
			}*/

			if( display != NULL )
			{
				char str[256];
				sprintf(str, "TensorRT build %x | %s | %04.1f FPS", NV_GIE_VERSION, net->HasFP16() ? "FP16" : "FP32", display->GetFPS());
				//sprintf(str, "GIE build %x | %s | %04.1f FPS | %05.2f%% %s", NV_GIE_VERSION, net->GetNetworkName(), display->GetFPS(), confidence * 100.0f, net->GetClassDesc(img_class));
				display->SetTitle(str);
			}
		}


		// update display
		if( display != NULL )
		{
			display->UserEvents();
			display->BeginRender();

			if( texture != NULL )
			{
				// rescale image pixel intensities for display
				CUDA(cudaNormalizeRGBA((float4*)imgRGBA, make_float2(0.0f, 255.0f),
								   (float4*)imgRGBA, make_float2(0.0f, 1.0f),
		 						   camera->GetWidth(), camera->GetHeight()));

				// map from CUDA to openGL using GL interop
				void* tex_map = texture->MapCUDA();

				if( tex_map != NULL )
				{
					cudaMemcpy(tex_map, imgRGBA, texture->GetSize(), cudaMemcpyDeviceToDevice);
					texture->Unmap();
				}

				// draw the texture
				texture->Render(100,100);
			}

			display->EndRender();
		}
	}


	/*
	 * shutdown the camera device
	 */
	if( camera != NULL )
	{
		delete camera;
		camera = NULL;
	}

	if( display != NULL )
	{
		delete display;
		display = NULL;
	}
	return 0;
}
