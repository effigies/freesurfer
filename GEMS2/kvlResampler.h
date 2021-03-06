#ifndef __kvlRegisterer_h
#define __kvlRegisterer_h

#include "itkImage.h"
#include "itkAffine3DTransform.h"


namespace kvl
{



/**
 *
 */
class Registerer: public itk::Object
{
public :
  
  /** Standard class typedefs */
  typedef Registerer  Self;
  typedef itk::Object  Superclass;
  typedef itk::SmartPointer< Self >  Pointer;
  typedef itk::SmartPointer< const Self >  ConstPointer;

  /** Method for creation through the object factory. */
  itkNewMacro( Self );

  /** Run-time type information (and related methods). */
  itkTypeMacro( Registerer, itk::Object );

  /** Some typedefs */
  typedef itk::Image< short, 3 >  ImageType;
  typedef itk::Affine3DTransform< double >   TransformType;
  typedef TransformType::ParametersType  ParametersType;

  // Set/get fixed image
  void SetFixedImage( const ImageType*  fixedImage )
    { m_FixedImage = fixedImage; }
  const ImageType*  GetFixedImage() const
    { return m_FixedImage; }

  // Set/get moving image
  void SetMovingImage( const ImageType*  movingImage )
    { m_MovingImage = movingImage; }
  const ImageType*  GetMovingImage() const
    { return m_MovingImage; }

  // 
  void  StartRegistration();

  //
  void  ApplyParameters( ImageType* image ) const
    {
    std::vector< ImageType::Pointer >  images;
    images.push_back( image );
    this->ApplyParameters( images );
    }

  void  ApplyParameters( std::vector< ImageType::Pointer > images ) const;

  // Set/Get
  void  SetParameters( const ParametersType& parameters )
    { m_Transform->SetParameters( parameters ); }

  const ParametersType&  GetParameters() const
    { return m_Transform->GetParameters(); }

protected:
  Registerer();
  virtual ~Registerer();


private:
  Registerer(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented

  ImageType::ConstPointer  m_FixedImage;
  ImageType::ConstPointer  m_MovingImage;

  TransformType::Pointer   m_Transform;


};


} // end namespace kvl

#endif
