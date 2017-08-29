#include "zstackprocessor.h"
#include "zstack.hxx"
#include "tz_stack_attribute.h"
#include "tz_stack_bwmorph.h"
#include "zspgrowparser.h"
#include "tz_stack_stat.h"
#include "tz_stack_lib.h"
#include "tz_stack_math.h"
#include "tz_utilities.h"
#include "tz_math.h"
#if _QS_LOG_AVAILABLE_
#  include "QsLog.h"
#endif
#include "tz_stack.h"
#include "tz_stack_math.h"
#include "tz_stack_neighborhood.h"
#include "tz_objdetect.h"
#include "tz_int_histogram.h"

ZStackProcessor::ZStackProcessor()
{
}

void ZStackProcessor::distanceTransform(ZStack *stack, bool isSquared,
                                        bool sliceWise)
{
  Stack *distanceMap = NULL;

  Stack *stackData = stack->c_stack();

  if (stack->kind() == GREY16) {
    Stack *newStackData = Translate_Stack(stackData, GREY, 0);
    if (sliceWise) {
      distanceMap = Stack_Bwdist_L_U16P(newStackData, NULL, 0);
    } else {
      distanceMap = Stack_Bwdist_L_U16(newStackData, NULL, 0);
    }
    Kill_Stack(newStackData);
  } else {
    if (sliceWise) {
      distanceMap = Stack_Bwdist_L_U16P(stack->c_stack(), NULL, 0);
    } else {
      distanceMap = Stack_Bwdist_L_U16(stack->c_stack(), NULL, 0);
    }
  }

  if (!isSquared) {
    uint16_t *array16 = (uint16_t*) distanceMap->array;

    size_t volume = Stack_Volume(distanceMap);

    for (size_t i = 0; i < volume; i++) {
      array16[i] = (uint16_t) fmin2(sqrt(array16[i]), 255.0f);
    }
  }

  stack->load(distanceMap, true);
}

void ZStackProcessor::shortestPathFlow(ZStack *stack)
{
  Stack *stackData = stack->c_stack();
  Stack *tmpdist = Stack_Bwdist_L_U16P(stackData, NULL, 0);
  Sp_Grow_Workspace *sgw = New_Sp_Grow_Workspace();
  sgw->wf = Stack_Voxel_Weight_I;
  size_t max_index;
  Stack_Max(tmpdist, &max_index);

  Stack *mask = Make_Stack(GREY, Stack_Width(tmpdist), Stack_Height(tmpdist),
                           Stack_Depth(tmpdist));
  Zero_Stack(mask);

  size_t nvoxel = Stack_Voxel_Number(stackData);
  size_t i;
  for (i = 0; i < nvoxel; i++) {
    if (stackData->array[i] == 0) {
      mask->array[i] = SP_GROW_BARRIER;
    }
  }

  mask->array[max_index] = SP_GROW_SOURCE;
  Sp_Grow_Workspace_Set_Mask(sgw, mask->array);
  Stack_Sp_Grow(tmpdist, NULL, 0, NULL, 0, sgw);

  ZSpGrowParser parser(sgw);
  stack->load(parser.createDistanceStack());

  Kill_Stack(mask);
  Kill_Stack(tmpdist);
}

void ZStackProcessor::mexihatFilter(ZStack *stack, double sigma)
{
  Stack *stackData = stack->c_stack();

  Filter_3d *filter = Mexihat_Filter_3d(sigma);

  Stack *filtered = Filter_Stack(stackData, filter);

  Kill_FMatrix(filter);

  stack->load(filtered, true);
}

void ZStackProcessor::expandRegion(ZStack *stack, int r)
{
  Stack *stackData = stack->c_stack();

  Stack *out = Stack_Region_Expand_M(stackData, 8, r, NULL, NULL);

  stack->load(out, true);
}

#if defined(_USE_ITK_)

#include <itkMedianImageFilter.h>
#include <itkCannyEdgeDetectionImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkRescaleIntensityImageFilter.h>
#include <itkGradientAnisotropicDiffusionImageFilter.h>
#include <itkCurvatureFlowImageFilter.h>
#include <itkMinMaxCurvatureFlowImageFilter.h>
#include <itkConnectedThresholdImageFilter.h>
#include <itkDiffusionTensor3D.h>
#include <itkMeasurementVectorTraits.h>
#include <itkGaussianRandomSpatialNeighborSubsampler.h>
#undef ASCII
#include <itkPatchBasedDenoisingImageFilter.h>

#define CONVERT_STACK(ImageType, VoxelType, ch)  \
  ImageType::RegionType region; \
  ImageType::SizeType size; \
  size[0] = stack->width(); \
  size[1] = stack->height(); \
  size[2] = stack->depth(); \
  region.SetSize(size); \
  image->SetRegions(size); \
  image->GetPixelContainer()-> \
  SetImportPointer((VoxelType*) stack->c_stack(ch)->array, \
  Stack_Voxel_Number(stack->c_stack(ch)));


void ZStackProcessor::convertStack(ZStack *stack,
                                   Uint8Image3DType *image)
{
  CONVERT_STACK(Uint8Image3DType, uint8, 0);
}

void ZStackProcessor::convertStack(ZStack *stack,
                                   Uint16Image3DType *image)
{
  CONVERT_STACK(Uint16Image3DType, uint16, 0);
}

void ZStackProcessor::convertStack(ZStack *stack,
                                   FloatImage3DType *image)
{
  CONVERT_STACK(FloatImage3DType, float32, 0);
}

void ZStackProcessor::copyData(Uint8Image3DType *src, ZStack *dst, int ch)
{
  uint8 *array = src->GetPixelContainer()->GetImportPointer();

  dst->copyValue(array, dst->getByteNumber(ZStack::SINGLE_CHANNEL), ch);
}

void ZStackProcessor::copyData(Uint16Image3DType *src, ZStack *dst, int ch)
{
  uint16 *array = src->GetPixelContainer()->GetImportPointer();

  dst->copyValue(array, dst->getByteNumber(ZStack::SINGLE_CHANNEL), ch);
}

void ZStackProcessor::copyData(FloatImage3DType *src, ZStack *dst, int ch)
{
  float *array = src->GetPixelContainer()->GetImportPointer();
  dst->copyValue(array, dst->getByteNumber(ZStack::SINGLE_CHANNEL), ch);
}

void ZStackProcessor::convertStack(const ZStack *stack, int ch, Uint8Image3DType *image)
{
  CONVERT_STACK(Uint8Image3DType, uint8, ch);
}

void ZStackProcessor::convertStack(const ZStack *stack, int ch, Uint16Image3DType *image)
{
  CONVERT_STACK(Uint16Image3DType, uint16, ch);
}

void ZStackProcessor::convertStack(const ZStack *stack, int ch, FloatImage3DType *image)
{
  CONVERT_STACK(FloatImage3DType, float32, ch);
}

void ZStackProcessor::medianFilter(ZStack *stack, int radius)
{
  if (!stack->isVirtual()) {
    switch (stack->data()->kind) {
    case GREY:
      {
        Uint8Image3DType::Pointer image = Uint8Image3DType::New();
        convertStack(stack, image);
        typedef itk::MedianImageFilter<Uint8Image3DType, Uint8Image3DType> FilterType;
        FilterType::Pointer filter = FilterType::New();

        Uint8Image3DType::SizeType indexRadius;
        indexRadius[0] = radius; // radius along x
        indexRadius[1] = radius; // radius along y
        indexRadius[2] = radius;
        filter->SetRadius( indexRadius );
        filter->SetInput(image);
        filter->Update();
        Uint8Image3DType::Pointer output = filter->GetOutput();
        copyData(output, stack);
        //stack->incrStamp();
      }
      break;
    case GREY16:
      {
        Uint16Image3DType::Pointer image = Uint16Image3DType::New();
        convertStack(stack, image);
        typedef itk::MedianImageFilter<Uint16Image3DType, Uint16Image3DType> FilterType;
        FilterType::Pointer filter = FilterType::New();

        Uint16Image3DType::SizeType indexRadius;
        indexRadius[0] = radius; // radius along x
        indexRadius[1] = radius; // radius along y
        indexRadius[2] = radius;
        filter->SetRadius( indexRadius );
        filter->SetInput(image);
        filter->Update();
        Uint16Image3DType::Pointer output = filter->GetOutput();
        copyData(output, stack);
        //stack->incrStamp();
      }
      break;
    default:
      break;
    }
  }
}

void ZStackProcessor::cannyEdge(ZStack *stack, double variance, double low,
                                double high)
{
  if (!stack->isVirtual()) {
    switch (stack->data()->kind) {
    case GREY:
      {
        Uint8Image3DType::Pointer image = Uint8Image3DType::New();
        convertStack(stack, image);

        typedef itk::CastImageFilter<Uint8Image3DType, DoubleImage3DType>
            CastToRealFilterType;
        typedef itk::RescaleIntensityImageFilter<DoubleImage3DType, Uint8Image3DType>
            RescaleFilter;

        CastToRealFilterType::Pointer toReal = CastToRealFilterType::New();
        RescaleFilter::Pointer rescale = RescaleFilter::New();

        toReal->SetInput(image);

        typedef itk::CannyEdgeDetectionImageFilter<DoubleImage3DType, DoubleImage3DType> FilterType;
        FilterType::Pointer canny = FilterType::New();

        canny->SetVariance(variance);
        canny->SetLowerThreshold(low);
        canny->SetUpperThreshold(high);

        canny->SetInput(toReal->GetOutput());
        rescale->SetInput(canny->GetOutput());

        rescale->Update();

        Uint8Image3DType::Pointer output = rescale->GetOutput();
        copyData(output, stack);
        //stack->incrStamp();
      }
      break;
    case GREY16:
      {
        Uint16Image3DType::Pointer image = Uint16Image3DType::New();
        convertStack(stack, image);

        typedef itk::CastImageFilter<Uint16Image3DType, DoubleImage3DType>
            CastToRealFilterType;
        typedef itk::RescaleIntensityImageFilter<DoubleImage3DType, Uint16Image3DType>
            RescaleFilter;

        CastToRealFilterType::Pointer toReal = CastToRealFilterType::New();
        RescaleFilter::Pointer rescale = RescaleFilter::New();

        toReal->SetInput(image);

        typedef itk::CannyEdgeDetectionImageFilter<DoubleImage3DType, DoubleImage3DType> FilterType;
        FilterType::Pointer canny = FilterType::New();

        canny->SetVariance(variance);
        canny->SetLowerThreshold(low);
        canny->SetUpperThreshold(high);

        canny->SetInput(toReal->GetOutput());
        rescale->SetInput(canny->GetOutput());

        rescale->Update();

        Uint16Image3DType::Pointer output = rescale->GetOutput();
        copyData(output, stack);
        //stack->incrStamp();
      }
      break;
    default:
      break;
    }
  }
}

void ZStackProcessor::anisotropicDiffusion(ZStack *stack, double timeStep,
                                           double conductance, int niter)
{
  if (!stack->isVirtual()) {
    switch (stack->data()->kind) {
    case GREY:
      {
        Uint8Image3DType::Pointer image = Uint8Image3DType::New();
        convertStack(stack, image);
        typedef itk::GradientAnisotropicDiffusionImageFilter<Uint8Image3DType, FloatImage3DType> FilterType;
        FilterType::Pointer filter = FilterType::New();
        typedef itk::RescaleIntensityImageFilter<FloatImage3DType, Uint8Image3DType> RescaleFilter;
        RescaleFilter::Pointer rescale = RescaleFilter::New();

        filter->SetConductanceParameter(conductance);
        filter->SetTimeStep(timeStep);
        filter->SetNumberOfIterations(niter);
        filter->SetInput(image);
        rescale->SetInput(filter->GetOutput());
        rescale->Update();
        Uint8Image3DType::Pointer output = rescale->GetOutput();
        copyData(output, stack);
    }
    break;
    case GREY16:
    {
      Uint16Image3DType::Pointer image = Uint16Image3DType::New();
      convertStack(stack, image);
      typedef itk::GradientAnisotropicDiffusionImageFilter<Uint16Image3DType, FloatImage3DType> FilterType;
      FilterType::Pointer filter = FilterType::New();
      typedef itk::RescaleIntensityImageFilter<FloatImage3DType, Uint16Image3DType> RescaleFilter;
      RescaleFilter::Pointer rescale = RescaleFilter::New();

      filter->SetConductanceParameter(20.0);
      filter->SetTimeStep(timeStep);
      filter->SetNumberOfIterations(niter);
      filter->SetInput(image);
      rescale->SetInput(filter->GetOutput());
      rescale->Update();
      Uint16Image3DType::Pointer output = rescale->GetOutput();
      copyData(output, stack);
    }
    break;
    default:
      break;
    }
    //stack->incrStamp();
  }
}

void ZStackProcessor::curvatureFlow(ZStack *stack, double timeStep, int niter)
{
  if (!stack->isVirtual()) {
    switch (stack->data()->kind) {
    case GREY:
      {
        Uint8Image3DType::Pointer image = Uint8Image3DType::New();
        convertStack(stack, image);
        typedef itk::CurvatureFlowImageFilter<Uint8Image3DType, FloatImage3DType> FilterType;
        FilterType::Pointer filter = FilterType::New();
        typedef itk::RescaleIntensityImageFilter<FloatImage3DType, Uint8Image3DType> RescaleFilter;
        RescaleFilter::Pointer rescale = RescaleFilter::New();

        filter->SetTimeStep(timeStep);
        filter->SetNumberOfIterations(niter);
        filter->SetInput(image);
        rescale->SetInput(filter->GetOutput());
        rescale->Update();
        Uint8Image3DType::Pointer output = rescale->GetOutput();
        copyData(output, stack);
    }
    break;
    case GREY16:
    {
      Uint16Image3DType::Pointer image = Uint16Image3DType::New();
      convertStack(stack, image);
      typedef itk::CurvatureFlowImageFilter<Uint16Image3DType, FloatImage3DType> FilterType;
      FilterType::Pointer filter = FilterType::New();
      typedef itk::RescaleIntensityImageFilter<FloatImage3DType, Uint16Image3DType> RescaleFilter;
      RescaleFilter::Pointer rescale = RescaleFilter::New();

      filter->SetTimeStep(timeStep);
      filter->SetNumberOfIterations(niter);
      filter->SetInput(image);
      rescale->SetInput(filter->GetOutput());
      rescale->Update();
      Uint16Image3DType::Pointer output = rescale->GetOutput();
      copyData(output, stack);
    }
    break;
    default:
      break;
    }
    //stack->incrStamp();
  }
}

void ZStackProcessor::minMaxCurvatureFlow(ZStack *stack, double timeStep,
                                          double radius, int niter)
{
#define MINMAXCURVATUREFLOW(ImageType) \
  { \
    ImageType::Pointer image = ImageType::New(); \
    convertStack(stack, image); \
    typedef itk::MinMaxCurvatureFlowImageFilter<ImageType, FloatImage3DType> FilterType; \
    FilterType::Pointer filter = FilterType::New(); \
    typedef itk::RescaleIntensityImageFilter<FloatImage3DType, ImageType> RescaleFilter; \
    RescaleFilter::Pointer rescale = RescaleFilter::New(); \
    filter->SetTimeStep(timeStep);  \
    filter->SetStencilRadius(radius); \
    filter->SetNumberOfIterations(niter); \
    filter->SetInput(image); \
    rescale->SetInput(filter->GetOutput()); \
    rescale->Update(); \
    ImageType::Pointer output = rescale->GetOutput(); \
    copyData(output, stack); \
  }

  if (!stack->isVirtual()) {
    switch (stack->data()->kind) {
    case GREY:
      MINMAXCURVATUREFLOW(Uint8Image3DType)
      break;
    case GREY16:
      MINMAXCURVATUREFLOW(Uint16Image3DType)
      break;
    break;
    default:
      break;
    }
  }
}

void ZStackProcessor::connectedThreshold(ZStack *stack, int x, int y, int z,
                                         int lower, int upper)
{
  if (!stack->isVirtual()) {
    switch (stack->data()->kind) {
    case GREY:
      {
        Uint8Image3DType::Pointer image = Uint8Image3DType::New();
        convertStack(stack, image);
        typedef itk::ConnectedThresholdImageFilter<Uint8Image3DType, Uint8Image3DType> FilterType;
        FilterType::Pointer filter = FilterType::New();
        itk::Index<3> index;
        index[0] = x;
        index[1] = y;
        index[2] = z;
        filter->SetSeed(index);
        filter->SetReplaceValue(1);
        filter->SetLower(lower);
        filter->SetUpper(upper);
        filter->SetInput(image);
        filter->Update();
        copyData(filter->GetOutput(), stack);
        //stack->incrStamp();
      }
      break;
    case GREY16:
      {
        Uint16Image3DType::Pointer image = Uint16Image3DType::New();
        convertStack(stack, image);
        typedef itk::ConnectedThresholdImageFilter<Uint16Image3DType, Uint16Image3DType> FilterType;
        FilterType::Pointer filter = FilterType::New();
        itk::Index<3> index;
        index[0] = x;
        index[1] = y;
        index[2] = z;
        filter->SetSeed(index);
        filter->SetReplaceValue(1);
        filter->SetLower(lower);
        filter->SetUpper(upper);
        filter->SetInput(image);
        filter->Update();
        copyData(filter->GetOutput(), stack);
        //stack->incrStamp();
      }
      break;
    default:
      break;
    }
    //stack->incrStamp();
  }
}

void ZStackProcessor::patchBasedDenoising(ZStack *stack, const int numIterations, const int numThreads,
                                          const int numToSample, const float sigmaMultiplicationFactor,
                                          const std::string noiseModel, const float fidelityWeight)
{
#if ITK_VERSION_MAJOR >= 4 && ITK_VERSION_MINOR >= 4
  if (!stack->isVirtual()) {
    switch (stack->data()->kind) {
    case GREY:
    {
      Uint8Image3DType::Pointer image = Uint8Image3DType::New();
      convertStack(stack, image);
      typedef itk::RescaleIntensityImageFilter<Uint8Image3DType, FloatImage3DType> RescaleFilter0;
      RescaleFilter0::Pointer rescale0 = RescaleFilter0::New();
      rescale0->SetOutputMinimum(0.f);
      rescale0->SetOutputMaximum(1.f);
      typedef itk::PatchBasedDenoisingImageFilter<FloatImage3DType, FloatImage3DType> FilterType;
      FilterType::Pointer filter = FilterType::New();
      typedef itk::RescaleIntensityImageFilter<FloatImage3DType, Uint8Image3DType> RescaleFilter;
      RescaleFilter::Pointer rescale = RescaleFilter::New();
      typedef itk::Statistics::GaussianRandomSpatialNeighborSubsampler<FilterType::PatchSampleType, FloatImage3DType::RegionType> SamplerType;

      // patch radius is same for all dimensions of the image
      const unsigned int patchRadius = 4;
      filter->SetPatchRadius(patchRadius);
      // instead of directly setting the weights, could also specify type
      filter->UseSmoothDiscPatchWeightsOn();
      //filter->UseFastTensorComputationsOn();

      // noise model to use
      if (noiseModel == "GAUSSIAN")
      {
        filter->SetNoiseModel(FilterType::GAUSSIAN);
      }
      else if (noiseModel == "RICIAN")
      {
        filter->SetNoiseModel(FilterType::RICIAN);
      }
      else if (noiseModel == "POISSON")
      {
        filter->SetNoiseModel(FilterType::POISSON);
      }
      // stepsize or weight for smoothing term
      // Large stepsizes may cause instabilities.
      filter->SetSmoothingWeight(1);
      // stepsize or weight for fidelity term
      // use a positive weight to prevent oversmoothing
      // (penalizes deviations from noisy data based on a noise model)
      filter->SetNoiseModelFidelityWeight(fidelityWeight);

      // number of iterations over the image of denoising
      filter->SetNumberOfIterations(numIterations);

      // number of threads to use in parallel
      filter->SetNumberOfThreads(numThreads);

      // sampling the image to find similar patches
      SamplerType::Pointer sampler = SamplerType::New();
      // variance (in physical units) for semi-local Gaussian sampling
      sampler->SetVariance(400);
      // rectangular window restricting the Gaussian sampling
      sampler->SetRadius(50); // 2.5 * standard deviation
      // number of random sample "patches" to use for computations
      sampler->SetNumberOfResultsRequested(numToSample);
      // Sampler can be complete neighborhood sampler, random neighborhood sampler,
      // Gaussian sampler, etc.
      filter->SetSampler(sampler);

      // automatic estimation of the kernel bandwidth
      filter->KernelBandwidthEstimationOn();
      // update bandwidth every 'n' iterations
      filter->SetKernelBandwidthUpdateFrequency(3);
      // use 33% of the pixels for the sigma update calculation
      filter->SetKernelBandwidthFractionPixelsForEstimation(0.20);
      // multiplication factor modifying the automatically-estimated kernel sigma
      filter->SetKernelBandwidthMultiplicationFactor(sigmaMultiplicationFactor);

      // manually-selected Gaussian kernel sigma
      // filter->DoKernelBandwidthEstimationOff();
      // typename FilterType::RealArrayType gaussianKernelSigma;
      // gaussianKernelSigma.SetSize(reader->GetOutput()->GetNumberOfComponentsPerPixel());
      // gaussianKernelSigma.Fill(11);
      // filter->SetGaussianKernelSigma (gaussianKernelSigma);


      rescale0->SetInput(image);
      filter->SetInput(rescale0->GetOutput());
      rescale->SetInput(filter->GetOutput());
      try {
        rescale->Update();
      }
      catch (itk::ExceptionObject & excp)
      {
        LERROR() << "Caught itk exception" << excp.GetDescription();
        return;
      }

      Uint8Image3DType::Pointer output = rescale->GetOutput();
      copyData(output, stack);
    }
    break;
    case GREY16:
    {
      Uint16Image3DType::Pointer image = Uint16Image3DType::New();
      convertStack(stack, image);
      typedef itk::RescaleIntensityImageFilter<Uint16Image3DType, FloatImage3DType> RescaleFilter0;
      RescaleFilter0::Pointer rescale0 = RescaleFilter0::New();
      rescale0->SetOutputMinimum(0.f);
      rescale0->SetOutputMaximum(1.f);
      typedef itk::PatchBasedDenoisingImageFilter<FloatImage3DType, FloatImage3DType> FilterType;
      FilterType::Pointer filter = FilterType::New();
      typedef itk::RescaleIntensityImageFilter<FloatImage3DType, Uint16Image3DType> RescaleFilter;
      RescaleFilter::Pointer rescale = RescaleFilter::New();
      typedef itk::Statistics::GaussianRandomSpatialNeighborSubsampler<FilterType::PatchSampleType, FloatImage3DType::RegionType> SamplerType;


      // patch radius is same for all dimensions of the image
      const unsigned int patchRadius = 4;
      filter->SetPatchRadius(patchRadius);
      // instead of directly setting the weights, could also specify type
      filter->UseSmoothDiscPatchWeightsOn();
      //filter->UseFastTensorComputationsOn();

      // noise model to use
      if (noiseModel == "GAUSSIAN")
      {
        filter->SetNoiseModel(FilterType::GAUSSIAN);
      }
      else if (noiseModel == "RICIAN")
      {
        filter->SetNoiseModel(FilterType::RICIAN);
      }
      else if (noiseModel == "POISSON")
      {
        filter->SetNoiseModel(FilterType::POISSON);
      }
      // stepsize or weight for smoothing term
      // Large stepsizes may cause instabilities.
      filter->SetSmoothingWeight(1);
      // stepsize or weight for fidelity term
      // use a positive weight to prevent oversmoothing
      // (penalizes deviations from noisy data based on a noise model)
      filter->SetNoiseModelFidelityWeight(fidelityWeight);

      // number of iterations over the image of denoising
      filter->SetNumberOfIterations(numIterations);

      // number of threads to use in parallel
      filter->SetNumberOfThreads(numThreads);

      // sampling the image to find similar patches
      SamplerType::Pointer sampler = SamplerType::New();
      // variance (in physical units) for semi-local Gaussian sampling
      sampler->SetVariance(400);
      // rectangular window restricting the Gaussian sampling
      sampler->SetRadius(50); // 2.5 * standard deviation
      // number of random sample "patches" to use for computations
      sampler->SetNumberOfResultsRequested(numToSample);
      // Sampler can be complete neighborhood sampler, random neighborhood sampler,
      // Gaussian sampler, etc.
      filter->SetSampler(sampler);

      // automatic estimation of the kernel bandwidth
      filter->KernelBandwidthEstimationOn();
      // update bandwidth every 'n' iterations
      filter->SetKernelBandwidthUpdateFrequency(3);
      // use 33% of the pixels for the sigma update calculation
      filter->SetKernelBandwidthFractionPixelsForEstimation(0.20);
      // multiplication factor modifying the automatically-estimated kernel sigma
      filter->SetKernelBandwidthMultiplicationFactor(sigmaMultiplicationFactor);

      // manually-selected Gaussian kernel sigma
      // filter->DoKernelBandwidthEstimationOff();
      // typename FilterType::RealArrayType gaussianKernelSigma;
      // gaussianKernelSigma.SetSize(reader->GetOutput()->GetNumberOfComponentsPerPixel());
      // gaussianKernelSigma.Fill(11);
      // filter->SetGaussianKernelSigma (gaussianKernelSigma);


      rescale0->SetInput(image);
      filter->SetInput(rescale0->GetOutput());
      rescale->SetInput(filter->GetOutput());
      try {
        rescale->Update();
      }
      catch (itk::ExceptionObject & excp)
      {
        LERROR() << "Caught itk exception" << excp.GetDescription();
        return;
      }

      Uint16Image3DType::Pointer output = rescale->GetOutput();
      copyData(output, stack);
    }
    break;
    default:
      break;
    }
    //stack->incrStamp();
  }
#else
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(numIterations);
  UNUSED_PARAMETER(numThreads);
  UNUSED_PARAMETER(numToSample);
  UNUSED_PARAMETER(sigmaMultiplicationFactor);
  UNUSED_PARAMETER(noiseModel[0]);
  UNUSED_PARAMETER(fidelityWeight);
#endif
}

#else

void ZStackProcessor::medianFilter(ZStack *stack, int radius)
{
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(radius);
}

void ZStackProcessor::anisotropicDiffusion(
    ZStack *stack, double timeStep, double conductance, int niter)
{
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(timeStep);
  UNUSED_PARAMETER(conductance);
  UNUSED_PARAMETER(niter);
}

void ZStackProcessor::cannyEdge(ZStack *stack, double variance,
                                double low, double high)
{
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(variance);
  UNUSED_PARAMETER(low);
  UNUSED_PARAMETER(high);
}

void ZStackProcessor::curvatureFlow(ZStack *stack, double timeStep, int niter)
{
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(timeStep);
  UNUSED_PARAMETER(niter);
}

void ZStackProcessor::minMaxCurvatureFlow(ZStack *stack, double timeStep,
                                          double radius, int niter)
{
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(timeStep);
  UNUSED_PARAMETER(radius);
  UNUSED_PARAMETER(niter);
}

void ZStackProcessor::convertStack(ZStack *stack, Uint8Image3DType *image)
{
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(image);
}

void ZStackProcessor::convertStack(ZStack *stack, Uint16Image3DType *image)
{
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(image);
}

void ZStackProcessor::convertStack(ZStack *stack, FloatImage3DType *image)
{
  UNUSED_PARAMETER(stack);
  UNUSED_PARAMETER(image);
}

void ZStackProcessor::copyData(Uint8Image3DType *src, ZStack *dst, int)
{
  UNUSED_PARAMETER(src);
  UNUSED_PARAMETER(dst);
}

void ZStackProcessor::copyData(Uint16Image3DType *src, ZStack *dst, int)
{
  UNUSED_PARAMETER(src);
  UNUSED_PARAMETER(dst);
}

void ZStackProcessor::copyData(FloatImage3DType *src, ZStack *dst, int)
{
  UNUSED_PARAMETER(src);
  UNUSED_PARAMETER(dst);
}

void ZStackProcessor::convertStack(const ZStack *, int, Uint8Image3DType *)
{
}

void ZStackProcessor::convertStack(const ZStack *, int, Uint16Image3DType *)
{
}

void ZStackProcessor::convertStack(const ZStack *, int, FloatImage3DType *)
{
}

void ZStackProcessor::patchBasedDenoising(ZStack *, const int, const int,
                                          const int, const float,
                                          const std::string, const float)
{
}

#endif

void ZStackProcessor::removeIsolatedObject(ZStack *stack, int r, int dr)
{
  Struct_Element *se = Make_Disc_Se(dr);

  Stack *out = Stack_Dilate(stack->c_stack(), NULL, se);

  Kill_Struct_Element(se);

  int minSize = iround(TZ_PI * (r + dr) * (r + dr));

  Stack *out2 = Stack_Remove_Small_Object(out, NULL, minSize, 8);

  Stack_And(out2, stack->c_stack(), stack->c_stack());

  //stack->incrStamp();

  Kill_Stack(out);
  Kill_Stack(out2);
}

void ZStackProcessor::invert(ZStack *stack)
{
  double maxValue = stack->max();
  for (int c = 0; c < stack->channelNumber(); ++c) {
    Stack_Csub(stack->c_stack(c), maxValue);
  }
}

void ZStackProcessor::flipY(ZStack *stack)
{
  C_Stack::flipY(stack->data(), stack->data());
}

void ZStackProcessor::SubtractBackground(ZStack *stack)
{
  for (int c = 0; c < stack->channelNumber(); ++c) {
    int commonIntensity = Stack_Common_Intensity(stack->c_stack(c), 0, 65535);
    Stack_Subc(stack->c_stack(c), commonIntensity);
  }
}

void ZStackProcessor::SubtractBackground(
    Stack *stackData, double minFr, int maxIter)
{
  ZIntHistogram *hist = C_Stack::hist(stackData, NULL);
  int maxV = hist->getMaxValue();

  int totalCount = hist->getUpperCount(0);

  int commonIntensity = 0;

  int darkCount = hist->getCount(0);
  if ((double) darkCount / totalCount > 0.9) {
    commonIntensity = hist->getMinValue();
  } else {
    for (int iter = 0; iter < maxIter; ++iter) {
      int mode = hist->getMode(commonIntensity + 1, maxV);
      if (mode == maxV) {
        break;
      }
      //      int commonIntensity =
      //          Stack_Common_Intensity(stack->c_stack(c), 0, 65535);

      commonIntensity = mode;
      double fgRatio = (double) hist->getUpperCount(commonIntensity + 1) /
          totalCount;
      if (fgRatio < minFr) {
        break;
      }
    }
  }

#ifdef _DEBUG_
  std::cout << "Subtracing background with value " << commonIntensity
            << std::endl;
#endif

  if (commonIntensity > 0) {
    Stack_Subc(stackData, commonIntensity);
  }

  delete hist;
}

void ZStackProcessor::SubtractBackground(ZStack *stack, double minFr, int maxIter)
{
  for (int c = 0; c < stack->channelNumber(); ++c) {
    Stack *stackData = stack->c_stack(c);
    SubtractBackground(stackData, minFr, maxIter);
  }
}

Stack* ZStackProcessor::GaussianSmooth(
    Stack *stack, double sx, double sy, double sz)
{
  Filter_3d *filter = Gaussian_Filter_3d(sx, sy, sz);
  Stack *out = Filter_Stack(stack, filter);
  Kill_FMatrix(filter);

  return out;
}

Stack* ZStackProcessor::PartialDiff(
    Stack *stack, double sx, double sy, double sz, NeuTube::EAxis axis)
{
  double scale[3];
  scale[0] = sx;
  scale[1] = sy;
  scale[2] = sz;

  FMatrix *filter = Gaussian_3D_Filter_F(scale, NULL);
  FMatrix *f = Filter_Stack_Fast_F(stack, filter, NULL, 0);

  Correct_Filter_Stack_F(filter, f);

  Kill_FMatrix(filter);

  FMatrix *result = NULL;
  switch (axis) {
  case NeuTube::X_AXIS:
    result = FMatrix_Partial_Diff(f, 0, NULL);
    break;
  case NeuTube::Y_AXIS:
    result = FMatrix_Partial_Diff(f, 1, NULL);
    break;
  case NeuTube::Z_AXIS:
    result = FMatrix_Partial_Diff(f, 2, NULL);
    break;
  }

  Stack *out = Scale_Float_Stack(result->array, result->dim[0], result->dim[1],
      result->dim[2], C_Stack::kind(stack));

  Kill_FMatrix(result);

  return out;
}

Stack* ZStackProcessor::PartialDiff(
    Stack *stack, double sx, double sy, double sz, NeuTube::EAxis axis1,
    NeuTube::EAxis axis2)
{
  double scale[3];
  scale[0] = sx;
  scale[1] = sy;
  scale[2] = sz;

  FMatrix *filter = Gaussian_3D_Filter_F(scale, NULL);
  FMatrix *f = Filter_Stack_Fast_F(stack, filter, NULL, 0);

  Correct_Filter_Stack_F(filter, f);

  Kill_FMatrix(filter);

  FMatrix *result = NULL;
  switch (axis1) {
  case NeuTube::X_AXIS:
    result = FMatrix_Partial_Diff(f, 0, NULL);
    break;
  case NeuTube::Y_AXIS:
    result = FMatrix_Partial_Diff(f, 1, NULL);
    break;
  case NeuTube::Z_AXIS:
    result = FMatrix_Partial_Diff(f, 2, NULL);
    break;
  }

  Kill_FMatrix(f);
  f = result;

  switch (axis2) {
  case NeuTube::X_AXIS:
    result = FMatrix_Partial_Diff(f, 0, NULL);
    break;
  case NeuTube::Y_AXIS:
    result = FMatrix_Partial_Diff(f, 1, NULL);
    break;
  case NeuTube::Z_AXIS:
    result = FMatrix_Partial_Diff(f, 2, NULL);
    break;
  }

  Stack *out = Scale_Float_Stack(result->array, result->dim[0], result->dim[1],
      result->dim[2], C_Stack::kind(stack));

  Kill_FMatrix(result);

  return out;
}

void ZStackProcessor::RemoveBranchPoint(Stack *stack, int nnbr)
{
  if (C_Stack::kind(stack) != GREY) {
    return;
  }

  int is_in_bound[26];
  int width = C_Stack::width(stack);
  int height = C_Stack::height(stack);
  int depth = C_Stack::depth(stack);

  int neighbor[26];
  Stack_Neighbor_Offset(nnbr, width, height, neighbor);
  size_t index;
  size_t voxelNumber = Stack_Voxel_Number(stack);

  for (index = 0; index < voxelNumber; ++index) {
    if (stack->array[index] > 0) {
      int n_in_bound = Stack_Neighbor_Bound_Test_I(nnbr, width, height, depth,
          index, is_in_bound);
      int j;
      int count = 0;
      for (j = 0; j < nnbr; ++j) {
        if (n_in_bound == nnbr || is_in_bound[j]) {
          size_t neighbor_index = index + neighbor[j];
          if (stack->array[neighbor_index] > 0) {
            ++count;
          }
        }
      }

      if (count > 2) {
        stack->array[index] = 0;
        for (j = 0; j < nnbr; ++j) {
          if (n_in_bound == nnbr || is_in_bound[j]) {
            size_t neighbor_index = index + neighbor[j];
            stack->array[neighbor_index] = 0;
          }
        }
      }
    }
  }
}

void ZStackProcessor::ShrinkSkeleton(Stack *stack, int level)
{
  for (int i = 0; i < level; ++i) {
    ShrinkSkeleton(stack);
  }
}

void ZStackProcessor::ShrinkSkeleton(Stack *stack)
{
  if (C_Stack::kind(stack) != GREY) {
    return;
  }

  std::vector<size_t> indexArray;
  size_t voxelNumber = C_Stack::voxelNumber(stack);
  for (size_t i = 0; i < voxelNumber; ++i) {
    if (stack->array[i] > 0) {
      int count = C_Stack::countForegoundNeighbor(stack, i, 8);
      if (count == 1) {
        indexArray.push_back(i);
      }
    }
  }

  for (std::vector<size_t>::const_iterator iter = indexArray.begin();
       iter != indexArray.end(); ++iter) {
    stack->array[*iter] = 0;
  }
}

static FMatrix* FMatrix_Eigen3_Line_Score(
    FMatrix *a, FMatrix *b, FMatrix *c, FMatrix *d,
    FMatrix *e, FMatrix *f, double alpha, FMatrix *result)
{
  if (result == NULL) {
    result = Make_FMatrix(a->dim, a->ndim);
  }

  size_t length = Matrix_Size(a->dim, a->ndim);
  size_t i;

  for (i = 0; i < length; i++) {
    if (a->array[i] + b->array[i] + c->array[i] >= 0.0) {
      result->array[i] = 0.0;
    } else {
      double coeff[3];
      coeff[0] = -a->array[i] - b->array[i] - c->array[i];
      coeff[1] = a->array[i] * b->array[i] +
          b->array[i] * c->array[i] +
          a->array[i] * c->array[i] - e->array[i] * e->array[i] -
          d->array[i] * d->array[i] - f->array[i] * f->array[i];
      coeff[2] = -a->array[i] * b->array[i] * c->array[i] -
          2.0 * d->array[i] * e->array[i] * f->array[i] +
          a->array[i] * f->array[i] * f->array[i] +
          c->array[i] * d->array[i] * d->array[i] +
          b->array[i] * e->array[i] * e->array[i];
      if (Solve_Cubic(1.0, coeff[0], coeff[1], coeff[2], coeff) > 0){
        double tmp;
        if (coeff[0] < coeff[1]) {
          SWAP2(coeff[0], coeff[1], tmp);
        }
        if (coeff[0] < coeff[2]) {
          SWAP2(coeff[0], coeff[2], tmp);
        }
        if (coeff[1] < coeff[2]) {
          SWAP2(coeff[1], coeff[2], tmp);
        }


        if ((coeff[1] >= 0) || (coeff[2] >= 0)) {
          result->array[i] = 0.0;
        } else {
          if (coeff[0] <= 0) {
            result->array[i] =
                sqrt(fabs(coeff[2] * (fabs(coeff[1]) + coeff[0])));
          } else if (coeff[0] < fabs(coeff[1]) / alpha) {
            result->array[i] =
                sqrt(fabs(coeff[2] * (fabs(coeff[1]) - alpha * coeff[0])));
          } else {
            result->array[i] = 0.0;
          }
        }
      } else {
        result->array[i] = 0.0;
      }
    }
  }

  return result;
}

static FMatrix* Line_Stack_F(
    const Stack *stack, const double *scale, FMatrix *result)
{
  FMatrix *filter = Gaussian_3D_Filter_F(scale, NULL);
  FMatrix *f = Filter_Stack_Fast_F(stack, filter, NULL, 0);

  Correct_Filter_Stack_F(filter, f);

  Kill_FMatrix(filter);

  FMatrix *Ix = FMatrix_Partial_Diff(f, 0, result);
  FMatrix *Ixx = FMatrix_Partial_Diff(Ix, 0, NULL);
  FMatrix *Ixy = FMatrix_Partial_Diff(Ix, 1, NULL);
  FMatrix *Ixz = FMatrix_Partial_Diff(Ix, 2, NULL);

  FMatrix *Iy = FMatrix_Partial_Diff(f, 1, Ix);
  FMatrix *Iyy = FMatrix_Partial_Diff(Iy, 1, NULL);
  FMatrix *Iyz = FMatrix_Partial_Diff(Iy, 2, NULL);

  FMatrix *Iz = FMatrix_Partial_Diff(f, 2, Iy);
  FMatrix *Izz = FMatrix_Partial_Diff(Iz, 2, f);

  if (result == NULL) {
    result = Ix;
  } else {
    Kill_FMatrix(Ix);
  }
  FMatrix_Eigen3_Line_Score(Ixx, Iyy, Izz, Ixy, Ixz, Iyz, 0.5, result);

  Kill_FMatrix(Ixx);
  Kill_FMatrix(Iyy);
  Kill_FMatrix(Izz);
  Kill_FMatrix(Ixy);
  Kill_FMatrix(Ixz);
  Kill_FMatrix(Iyz);

  return result;
}

Stack* ZStackProcessor::EnhanceLine(
    Stack *stack, double sigmaX, double sigmaY, double sigmaZ)
{
  double scale[3];
  scale[0] = sigmaX;
  scale[1] = sigmaY;
  scale[2] = sigmaZ;

  FMatrix *result = Line_Stack_F(stack, scale, NULL);

  Stack *out = Scale_Float_Stack(result->array, result->dim[0], result->dim[1],
      result->dim[2], C_Stack::kind(stack));

  Kill_FMatrix(result);

  return out;
}

/*
void ZStackProcessor::SubtractBackground(Stack *stack, double minFr, int maxIter)
{
  int *hist = Stack_Hist(stack);
  int minV = Int_Histogram_Min(hist);
  int maxV = Int_Histogram_Max(hist);

  int commonIntensity = 0;

  if ((double) hist[2] / Stack_Voxel_Number(stack) > 0.9) {
    commonIntensity = minV;
  } else {
    for (int iter = 0; iter < maxIter; ++iter) {
      int mode =  Int_Histogram_Mode(hist, commonIntensity + 1, maxV);
      if (mode == maxV) {
        break;
      }

      commonIntensity = mode;
      double fgRatio = (double) HistUpperCount(hist, commonIntensity + 1) /
          Stack_Voxel_Number(stack);
      if (fgRatio < minFr) {
        break;
      }
    }
  }

  std::cout << "Subtracing background: " << commonIntensity << std::endl;
  Stack_Subc(stack, commonIntensity);
}
*/
