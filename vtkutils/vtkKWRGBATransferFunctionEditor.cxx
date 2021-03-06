/**
 * @file  vtkKWRGBATransferFunctionEditor.cxx
 * @brief an RGBA tfunc function editor
 *
 * A widget that allows the user to edit a color transfer
 * function. Note that as a subclass of
 * vtkKWParameterValueFunctionEditor, since the 'value' range is
 * multi-dimensional (r, g, b), this widget only allows the
 * 'parameter' of a function point to be changed (i.e., a point can
 * only be moved horizontally). This modifed version of
 * vtkKWColorTransferFunctionEditor will properly over the fourth
 * alpha component. There is no control in the editor to set alpha,
 * but you can set it manually from the code, and the editor won't
 * mess with it.
 */
/*
 * Original Author: Kitware, Inc, modified by Kevin Teich
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/02 00:04:56 $
 *    $Revision: 1.8 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */


/*=========================================================================

  Module:    $RCSfile: vtkKWRGBATransferFunctionEditor.cxx,v $

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

  Modified by Kevin Teich

=========================================================================*/
#include "vtkKWRGBATransferFunctionEditor.h"

#include "vtkRGBATransferFunction.h"
#include "vtkKWEntry.h"
#include "vtkKWFrame.h"
#include "vtkKWHistogram.h"
#include "vtkKWLabel.h"
#include "vtkKWEntryWithLabel.h"
#include "vtkKWInternationalization.h"
#include "vtkKWScaleWithEntry.h"
#include "vtkKWMenu.h"
#include "vtkKWMenuButton.h"
#include "vtkKWRange.h"
#include "vtkKWCanvas.h"
#include "vtkKWTkUtilities.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"

#include <vtksys/stl/string>

vtkStandardNewMacro(vtkKWRGBATransferFunctionEditor);
vtkCxxRevisionMacro(vtkKWRGBATransferFunctionEditor, "$Revision: 1.8 $");

#define VTK_KW_CTFE_COLOR_RAMP_TAG "color_ramp_tag"

#define VTK_KW_CTFE_NB_ENTRIES 3

#define VTK_KW_CTFE_COLOR_RAMP_HEIGHT_MIN 2

//----------------------------------------------------------------------------
vtkKWRGBATransferFunctionEditor::vtkKWRGBATransferFunctionEditor() {
  this->RGBATransferFunction          = NULL;
  this->ColorRampTransferFunction      = NULL;

  this->ComputePointColorFromValue     = 1;
  this->ComputeHistogramColorFromValue = 0;
  this->ValueEntriesVisibility               = 1;
  this->ColorSpaceOptionMenuVisibility       = 1;
  this->ColorRampVisibility                  = 1;
  this->ColorRampHeight                = 10;
  this->LastRedrawColorRampTime        = 0;
  this->ColorRampPosition              = vtkKWRGBATransferFunctionEditor::ColorRampPositionDefault;
  this->ColorRampOutlineStyle          = vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleSolid;

  this->ColorSpaceOptionMenu           = vtkKWMenuButton::New();
  this->ColorRamp                      = vtkKWLabel::New();

  this->PointCountMinimum              = -1;
  this->PointCountMaximum              = -1;

  int i;
  for (i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
    this->ValueEntries[i] = vtkKWEntryWithLabel::New();
  }

  this->UpdateDepth = 0;
  this->DontUpdateSticky = false;

  this->ValueRangeVisibilityOff();
}

//----------------------------------------------------------------------------
vtkKWRGBATransferFunctionEditor::~vtkKWRGBATransferFunctionEditor() {
  if (this->ColorSpaceOptionMenu) {
    this->ColorSpaceOptionMenu->Delete();
    this->ColorSpaceOptionMenu = NULL;
  }

  if (this->ColorRamp) {
    this->ColorRamp->Delete();
    this->ColorRamp = NULL;
  }

  int i;
  for (i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
    if (this->ValueEntries[i]) {
      this->ValueEntries[i]->Delete();
      this->ValueEntries[i] = NULL;
    }
  }

  this->SetRGBATransferFunction(NULL);
  this->SetColorRampTransferFunction(NULL);
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetRGBATransferFunction(
  vtkRGBATransferFunction *arg) {
  if (this->RGBATransferFunction == arg) {
    return;
  }

  if (this->RGBATransferFunction) {
    this->RGBATransferFunction->UnRegister(this);
  }

  this->RGBATransferFunction = arg;

  this->LastRedrawFunctionTime = 0;

  // If we are using this function to color the ramp, then reset that time
  // too (otherwise leave that to SetColorRampTransferFunction)

  if (!this->ColorRampTransferFunction) {
    this->LastRedrawColorRampTime = 0;
  }

  if (this->RGBATransferFunction) {
    this->RGBATransferFunction->Register(this);
    this->SetWholeParameterRangeToFunctionRange();
  }

  this->Modified();

  this->Update();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetColorRampTransferFunction(
  vtkRGBATransferFunction *arg) {
  if (this->ColorRampTransferFunction == arg) {
    return;
  }

  if (this->ColorRampTransferFunction) {
    this->ColorRampTransferFunction->UnRegister(this);
  }

  this->ColorRampTransferFunction = arg;

  if (this->ColorRampTransferFunction) {
    this->ColorRampTransferFunction->Register(this);
  }

  this->Modified();

  this->LastRedrawColorRampTime = 0;

  this->RedrawColorRamp();
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::HasFunction() {
  return this->RGBATransferFunction ? 1 : 0;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::GetFunctionSize() {
  return this->RGBATransferFunction ?
         this->RGBATransferFunction->GetSize() : 0;
}

//----------------------------------------------------------------------------
unsigned long vtkKWRGBATransferFunctionEditor::GetFunctionMTime() {
  return this->RGBATransferFunction ?
         this->RGBATransferFunction->GetMTime() : 0;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::GetFunctionPointParameter(
  int id, double *parameter) {
  if (!this->HasFunction() || id < 0 || id >= this->GetFunctionSize()) {
    return 0;
  }

  *parameter = this->RGBATransferFunction->GetDataPointer()[
                 id * (1 + this->GetFunctionPointDimensionality())];

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::GetFunctionPointDimensionality() {
  return 4;
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetPointCountMinimum ( int min ) {
  this->PointCountMinimum = min;
}

void vtkKWRGBATransferFunctionEditor::SetPointCountMaximum ( int max ) {
  this->PointCountMaximum = max;
}

void vtkKWRGBATransferFunctionEditor::SetPointSymmetry ( int id1, int id2 ) {
  this->PointSymmetry[id1] = id2;
  this->PointSymmetry[id2] = id1;
}

void vtkKWRGBATransferFunctionEditor::SetPointSticky ( int id1, int id2 ) {
  this->PointSticky[id1] = id2;
  this->PointSticky[id2] = id1;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::GetFunctionPointValues(
  int id, double *values) {
  if (!this->HasFunction() || id < 0 || id >= this->GetFunctionSize() ||
      !values) {
    return 0;
  }

  int dim = this->GetFunctionPointDimensionality();

  memmove(values,
         (this->RGBATransferFunction->GetDataPointer() + id * (1 + dim) + 1),
         dim * sizeof(double));

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::SetFunctionPointValues(
  int id, const double *values) {
  double parameter;
  if (!values || !this->GetFunctionPointParameter(id, &parameter)) {
    return 0;
  }

  // Clamp the paramater to the whole range, first three values (RGB)
  // to the whole value range, and the last value (alpha) to 0..1.
  vtkMath::ClampValue(&parameter, this->GetWholeParameterRange());
  double clamped_values[
    vtkKWParameterValueFunctionEditor::MaxFunctionPointDimensionality];
  vtkMath::ClampValues(values, 3, this->GetWholeValueRange(), clamped_values);
  double alphaRange[2] = {0, 1};
  vtkMath::ClampValues(&values[3], 1, alphaRange, &clamped_values[3]);

  // This is using the AddRGBAPoint function, but is actually just
  // adding a point with a value that is already present, so it's only
  // really setting the value.
  this->RGBATransferFunction->AddRGBAPoint(
    parameter,
    clamped_values[0], clamped_values[1], clamped_values[2], clamped_values[3]);

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::InterpolateFunctionPointValues(
  double parameter, double *values) {
  if (!this->HasFunction() || !values) {
    return 0;
  }

  this->RGBATransferFunction->GetColor(parameter, values);

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::AddFunctionPoint(
  double parameter, const double *values, int *id) {
  if (!this->HasFunction() || !values || !id) {
    return 0;
  }

  // Make sure we aren't already at our max number of points.
  if (this->RGBATransferFunction->GetSize() >= this->PointCountMaximum) {
    return 0;
  }

  // Clamp the first three values (RGB) to the whole value range, and
  // the last value (alpha) to 0..1.
  double clamped_values[
    vtkKWParameterValueFunctionEditor::MaxFunctionPointDimensionality];
  vtkMath::ClampValues(values, 3, this->GetWholeValueRange(), clamped_values);
  double alphaRange[2] = {0, 1};
  vtkMath::ClampValues(&values[3], 1, alphaRange, &clamped_values[3]);

  // Add the point

  int old_size = this->GetFunctionSize();
  *id = this->RGBATransferFunction->
    AddRGBAPoint( parameter, 
		  clamped_values[0], clamped_values[1], clamped_values[2],
		  clamped_values[3] );
  
  return (old_size != this->GetFunctionSize());
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::SetFunctionPoint(
  int id, double parameter, const double *values) {
  if (!this->HasFunction() || !values) {
    return 0;
  }

  double old_parameter;
  if (!this->GetFunctionPointParameter(id, &old_parameter)) {
    return 0;
  }

  // If this point is supposed to be symmetrical with something, make
  // sure it doesn't cross the symmetry point.
  if ( this->PointSymmetry.find(id) != this->PointSymmetry.end() ) {

    if ( (old_parameter < 0 && parameter >= 0) ||
         (old_parameter > 0 && parameter <= 0) ) {

      return 0;
    }
  }

  this->UpdateDepth++;

  // We'll use AddRGBAPoint to add the point, but since we're
  // specifying an id, we check to see that value at that id, and
  // remove the id if it isn't the same.

  // Clamp the paramater to the whole range, first three values (RGB)
  // to the whole value range, and the last value (alpha) to 0..1.
  vtkMath::ClampValue(&parameter, this->GetWholeParameterRange());
  double clamped_values[
    vtkKWParameterValueFunctionEditor::MaxFunctionPointDimensionality];
  vtkMath::ClampValues(values, 3, this->GetWholeValueRange(), clamped_values);
  double alphaRange[2] = {0, 1};
  vtkMath::ClampValues(&values[3], 1, alphaRange, &clamped_values[3]);

  if (parameter != old_parameter) {
    this->RGBATransferFunction->RemovePoint(old_parameter);
  }
  int new_id = this->RGBATransferFunction->
    AddRGBAPoint( parameter,
		  clamped_values[0], clamped_values[1], clamped_values[2], 
		  clamped_values[3] );

  if (new_id != id) {
    vtkWarningMacro(<< "Setting a function point (id: " << id << ") parameter/values resulted in a different point (id:" << new_id << "). Inconsistent.");
    return 0;
  }

  if( this->UpdateDepth < 3 ) {
    UpdateSymmetricalPointsTo( id );
    UpdateStickyPointsTo( id, parameter - old_parameter );
  }

  this->UpdateDepth--;

  return 1;
}

void vtkKWRGBATransferFunctionEditor::UpdateSymmetricalPointsTo ( int id ) {

  // Set the symmetric point if available.
  if ( this->PointSymmetry.find(id) != this->PointSymmetry.end() ) {

    int symmetric_id = this->PointSymmetry[id];

    double parameter;
    this->GetFunctionPointParameter(id, &parameter);

    double old_parameter;
    this->GetFunctionPointParameter(symmetric_id, &old_parameter);

    double new_parameter = -parameter;
    
    double symmetric_values
      [vtkKWParameterValueFunctionEditor::MaxFunctionPointDimensionality];
    this->GetFunctionPointValues( symmetric_id, symmetric_values );

    // Move the point.
    this->MoveFunctionPointInColorSpace( symmetric_id, 
					 new_parameter, symmetric_values,
			 this->RGBATransferFunction->GetColorSpace());

    // Set the sticky point if available.
    UpdateStickyPointsTo( symmetric_id, new_parameter - old_parameter );
  }
}

void vtkKWRGBATransferFunctionEditor::UpdateStickyPointsTo ( int id, double delta ) {

  if( this->DontUpdateSticky )
    return;

  // Set the sticky point if available.
  if ( this->PointSticky.find(id) != this->PointSticky.end() ) {

    int sticky_id = this->PointSticky[id];

    double parameter;
    this->GetFunctionPointParameter(id, &parameter);

    double old_parameter;
    this->GetFunctionPointParameter(sticky_id, &old_parameter);

    double new_parameter = old_parameter + delta;
    if( sticky_id < id ) 
      new_parameter = parameter-0.0001;
    else
      new_parameter = parameter+0.0001;

    double sticky_values
      [vtkKWParameterValueFunctionEditor::MaxFunctionPointDimensionality];
    this->GetFunctionPointValues( sticky_id, sticky_values );

    // Move the point.
    this->DontUpdateSticky = true;
    this->MoveFunctionPointInColorSpace( sticky_id, 
					 new_parameter, sticky_values,
			 this->RGBATransferFunction->GetColorSpace());
    this->DontUpdateSticky = false;

    // Set the symmetrical point if available.
    UpdateSymmetricalPointsTo( sticky_id );
  }
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::RemoveFunctionPoint(int id) {
  if (!this->HasFunction() || id < 0 || id >= this->GetFunctionSize()) {
    return 0;
  }

  // Make sure we aren't already at our max number of points.
  if (this->RGBATransferFunction->GetSize() -1 < this->PointCountMinimum) {
    return 0;
  }


  // Remove the point
  double parameter = this->RGBATransferFunction->GetDataPointer()[
                       id * (1 + this->GetFunctionPointDimensionality())];

  int old_size = this->GetFunctionSize();
  this->RGBATransferFunction->RemovePoint(parameter);
  return (old_size != this->GetFunctionSize());
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::GetFunctionPointMidPoint(
  int id, double *pos) {
  if (id < 0 || id >= this->GetFunctionSize() || !pos) {
    return 0;
  }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::SetFunctionPointMidPoint(
  int id, double pos) {
  if (id < 0 || id >= this->GetFunctionSize()) {
    return 0;
  }

  if (pos < 0.0) {
    pos = 0.0;
  } else if (pos > 1.0) {
    pos = 1.0;
  }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::GetFunctionPointSharpness(
  int id, double *sharpness) {
  if (id < 0 || id >= this->GetFunctionSize() || !sharpness) {
    return 0;
  }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::SetFunctionPointSharpness(
  int id, double sharpness) {
  if (id < 0 || id >= this->GetFunctionSize()) {
    return 0;
  }

  if (sharpness < 0.0) {
    sharpness = 0.0;
  } else if (sharpness > 1.0) {
    sharpness = 1.0;
  }

  return 0;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::MoveFunctionPointInColorSpace(
  int id, double parameter, const double *values, int colorspace) {
  // RGB space is native space, so use the superclass default implem

  if (colorspace == VTK_CTF_RGB) {
    return this->Superclass::MoveFunctionPoint(id, parameter, values);
  }

  // Otherwise convert from HSV to RGB

  double rgb[3];
  vtkMath::HSVToRGB(values[0], values[1], values[2], rgb, rgb + 1, rgb + 2);
  return this->Superclass::MoveFunctionPoint(id, parameter, rgb);

  // The old implementation used to work differently: instead of converting
  // the values to RGB space, we would try to stay in HSV space as much as
  // possible, by getting the previous values for that point in HSV space.
  // I don't remember why that choice, it seems to work with the above
  // implem. If the problem comes back, check the CVS.

  /*
    if (colorspace == VTK_CTF_HSV)
    {
    vtkMath::RGBToHSV(old_values, hsv);
    old_values = hsv;
    }

    [...]

    double values_in_rgb[3];
    if (colorspace == VTK_CTF_HSV)
    {
    vtkMath::HSVToRGB(values, values_in_rgb);
    values = values_in_rgb;
    }
  */
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::UpdatePointEntries(
  int id) {
  this->Superclass::UpdatePointEntries(id);

  if (!this->IsCreated()) {
    return;
  }

  int i;

  // No point ? Empty the entries and disable

  if (!this->HasFunction() || id < 0 || id >= this->GetFunctionSize()) {
    for (i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
      if (this->ValueEntries[i]) {
        this->ValueEntries[i]->GetWidget()->SetValue("");
        this->ValueEntries[i]->SetEnabled(0);
      }
    }
    return;
  }

  // Disable entries if value is locked

  for (i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
    this->ValueEntries[i]->SetEnabled(
      this->FunctionPointValueIsLocked(id) ? 0 : this->GetEnabled());
  }

  // Get the values in the right color space
  double *node_value = this->RGBATransferFunction->GetDataPointer() + id * 4;

  double *values = node_value + 1, hsv[3];
  if (this->RGBATransferFunction->GetColorSpace() == VTK_CTF_HSV) {
    vtkMath::RGBToHSV(values, hsv);
    values = hsv;
  }

  for (i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
    this->ValueEntries[i]->GetWidget()->SetValueAsFormattedDouble(
      values[i], 2);
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::UpdatePointEntriesLabel() {
  if (VTK_KW_CTFE_NB_ENTRIES != 3 ||
      !this->RGBATransferFunction ||
      (this->RGBATransferFunction->GetColorSpace() != VTK_CTF_HSV &&
       this->RGBATransferFunction->GetColorSpace() != VTK_CTF_RGB)) {
    for (int i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
      if (this->ValueEntries[i]) {
        this->ValueEntries[i]->GetLabel()->SetText("");
      }
    }
    return;
  }

  if (this->RGBATransferFunction) {
    if (this->RGBATransferFunction->GetColorSpace() == VTK_CTF_HSV) {
      if (this->ValueEntries[0]) {
        this->ValueEntries[0]->GetLabel()->SetText(ks_("Color Space|Hue|H:"));
      }
      if (this->ValueEntries[1]) {
        this->ValueEntries[1]->GetLabel()->SetText(ks_("Color Space|Saturation|S:"));
      }
      if (this->ValueEntries[2]) {
        this->ValueEntries[2]->GetLabel()->SetText(ks_("Color Space|Value|V:"));
      }
    } else if (this->RGBATransferFunction->GetColorSpace() == VTK_CTF_RGB) {
      if (this->ValueEntries[0]) {
        this->ValueEntries[0]->GetLabel()->SetText(ks_("Color Space|Red|R:"));
      }
      if (this->ValueEntries[1]) {
        this->ValueEntries[1]->GetLabel()->SetText(ks_("Color Space|Green|G:"));
      }
      if (this->ValueEntries[2]) {
        this->ValueEntries[2]->GetLabel()->SetText(ks_("Color Space|Blue|B:"));
      }
    }
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::UpdateColorSpaceOptionMenu() {
  if (!this->IsCreated()) {
    return;
  }

  if (this->ColorSpaceOptionMenu && this->RGBATransferFunction) {
    switch (this->RGBATransferFunction->GetColorSpace()) {
    case VTK_CTF_HSV:
      if (this->RGBATransferFunction->GetHSVWrap())
        this->ColorSpaceOptionMenu->SetValue(ks_("Color Space|HSV"));
      else
        this->ColorSpaceOptionMenu->SetValue(ks_("Color Space|HSV (2)"));
      break;
    default:
    case VTK_CTF_RGB:
      this->ColorSpaceOptionMenu->SetValue(ks_("Color Space|RGB"));
      break;
    }
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::CreateWidget() {
  // Check if already created

  if (this->IsCreated()) {
    vtkErrorMacro("RGBATransferFunctionEditor already created");
    return;
  }

  // Call the superclass to create the whole widget

  this->Superclass::CreateWidget();

  // Add the color space option menu

  if (this->ColorSpaceOptionMenuVisibility) {
    this->CreateColorSpaceOptionMenu();
  }

  // Create the value entries

  if (this->ValueEntriesVisibility && this->PointEntriesVisibility) {
    this->CreateValueEntries();
  }

  // Create the ramp

  if (this->ColorRampVisibility) {
    this->CreateColorRamp();
  }

  // Pack the widget

  this->Pack();

  // Update

  this->Update();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::CreateColorSpaceOptionMenu() {
  if (this->ColorSpaceOptionMenu && !this->ColorSpaceOptionMenu->IsCreated()) {
    this->CreateTopLeftFrame();
    this->ColorSpaceOptionMenu->SetParent(this->TopLeftFrame);
    this->ColorSpaceOptionMenu->Create();
    this->ColorSpaceOptionMenu->SetPadX(1);
    this->ColorSpaceOptionMenu->SetPadY(1);
    this->ColorSpaceOptionMenu->IndicatorVisibilityOff();
    this->ColorSpaceOptionMenu->SetBalloonHelpString(
      k_("Change the interpolation color space to RGB or HSV."));

    const char callback[] = "ColorSpaceCallback";

    vtkKWMenu *menu = this->ColorSpaceOptionMenu->GetMenu();
    menu->AddRadioButton(ks_("Color Space|RGB"), this, callback);
    menu->AddRadioButton(ks_("Color Space|HSV"), this, callback);
    menu->AddRadioButton(ks_("Color Space|HSV (2)"), this, callback);

    this->UpdateColorSpaceOptionMenu();
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::CreateColorRamp() {
  if (this->ColorRamp && !this->ColorRamp->IsCreated()) {
    this->ColorRamp->SetParent(this);
    this->ColorRamp->Create();
    this->ColorRamp->SetBorderWidth(0);
    this->ColorRamp->SetAnchorToNorthWest();
    if (!this->IsColorRampUpToDate()) {
      this->RedrawColorRamp();
    }
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::CreateValueEntries() {
  if (this->ValueEntries[0] && !this->ValueEntries[0]->IsCreated()) {
    this->CreatePointEntriesFrame();
    int i;
    for (i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
      this->ValueEntries[i]->SetParent(this->PointEntriesFrame);
      this->ValueEntries[i]->Create();
      this->ValueEntries[i]->GetWidget()->SetWidth(4);
      this->ValueEntries[i]->GetWidget()->SetCommand(
        this, "ValueEntriesCallback");
    }

    this->UpdatePointEntriesLabel();
    this->UpdatePointEntries(this->GetSelectedPoint());
  }
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::IsTopLeftFrameUsed() {
  return (this->Superclass::IsTopLeftFrameUsed() ||
          this->ColorSpaceOptionMenuVisibility);
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::IsPointEntriesFrameUsed() {
  return (this->Superclass::IsPointEntriesFrameUsed() ||
          (this->PointEntriesVisibility && this->ValueEntriesVisibility));
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::Pack() {
  if (!this->IsCreated()) {
    return;
  }

  // Pack the whole widget

  this->Superclass::Pack();

  ostrstream tk_cmd;

  // Add the color space menu (in top left frame)

  if (this->ColorSpaceOptionMenuVisibility &&
      this->ColorSpaceOptionMenu && this->ColorSpaceOptionMenu->IsCreated()) {
    tk_cmd << "pack " << this->ColorSpaceOptionMenu->GetWidgetName()
    << " -side left -fill both -padx 0" << endl;
  }

  // Color ramp

  if (this->ColorRampVisibility &&
      (this->ColorRampPosition ==
       vtkKWRGBATransferFunctionEditor::ColorRampPositionDefault) &&
      this->ColorRamp && this->ColorRamp->IsCreated()) {
    // Get the current position of the parameter range, and move it one
    // row below. Otherwise get the current number of rows and insert
    // the ramp at the end

    int show_pr =
      (this->ParameterRangeVisibility &&
       this->ParameterRange && this->ParameterRange->IsCreated()) ? 1 : 0;

    int col, row, nb_cols;
    if (show_pr &&
        (this->ParameterRangePosition ==
         vtkKWParameterValueFunctionEditor::ParameterRangePositionBottom) &&
        vtkKWTkUtilities::GetWidgetPositionInGrid(
          this->ParameterRange, &col, &row)) {
      tk_cmd << "grid " << this->ParameterRange->GetWidgetName()
      << " -row " << row + 1 << endl;
    } else {
      col = 2;
      if (!vtkKWTkUtilities::GetGridSize(
            this->ColorRamp->GetParent(), &nb_cols, &row)) {
        row = 2 + (this->ParameterTicksVisibility ? 1 : 0) +
              (show_pr &&
               (this->ParameterRangePosition ==
                vtkKWParameterValueFunctionEditor::ParameterRangePositionTop)
               ? 1 :0 );
      }
    }
    tk_cmd << "grid " << this->ColorRamp->GetWidgetName()
    << " -columnspan 2 -sticky w -padx 0 "
    << " -pady " << (this->CanvasVisibility ? 2 : 0)
    << " -column " << col << " -row " << row << endl;
  }

  tk_cmd << ends;
  this->Script(tk_cmd.str());
  tk_cmd.rdbuf()->freeze(0);
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::PackPointEntries() {
  if (!this->IsCreated()) {
    return;
  }

  // Pack the other entries

  this->Superclass::PackPointEntries();

  ostrstream tk_cmd;

  // Value entries (in top right frame)

  if (this->HasSelection() &&
      this->ValueEntriesVisibility &&
      this->PointEntriesVisibility) {
    for (int i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
      if (this->ValueEntries[i] && this->ValueEntries[i]->IsCreated()) {
        tk_cmd << "pack " << this->ValueEntries[i]->GetWidgetName()
        << " -side left -pady 0" << endl;
      }
    }
  }

  tk_cmd << ends;
  this->Script(tk_cmd.str());
  tk_cmd.rdbuf()->freeze(0);
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::Update() {
  this->Superclass::Update();

  if (!this->IsCreated()) {
    return;
  }

  // Update the color space menu the reflect the current color space
  // And the entries

  this->UpdateColorSpaceOptionMenu();

  this->UpdatePointEntriesLabel();

  // No selection, disable value entries

  if (!this->HasSelection()) {
    for (int i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
      if (this->ValueEntries[i]) {
        this->ValueEntries[i]->SetEnabled(0);
      }
    }
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::UpdateEnableState() {
  this->Superclass::UpdateEnableState();

  this->PropagateEnableState(this->ColorSpaceOptionMenu);

  for (int i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
    this->PropagateEnableState(this->ValueEntries[i]);
  }

  this->PropagateEnableState(this->ColorRamp);
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::ColorSpaceCallback() {
  if (this->RGBATransferFunction) {
    const char * value = this->ColorSpaceOptionMenu->GetValue();
    if ( strcmp(value, ks_("Color Space|RGB")) == 0) {
      if ( this->RGBATransferFunction->GetColorSpace() != VTK_CTF_RGB ) {
        this->RGBATransferFunction->SetColorSpace( VTK_CTF_RGB );
        this->Update();
        if (this->HasSelection()) {
          this->UpdatePointEntries(this->GetSelectedPoint());
        }
        this->InvokeFunctionChangedCommand();
      }
    } else if ( strcmp(value, ks_("Color Space|HSV")) == 0) {
      if ( this->RGBATransferFunction->GetColorSpace() != VTK_CTF_HSV ||
           !this->RGBATransferFunction->GetHSVWrap() ) {
        this->RGBATransferFunction->SetColorSpace( VTK_CTF_HSV );
        this->RGBATransferFunction->HSVWrapOn();
        this->Update();
        if (this->HasSelection()) {
          this->UpdatePointEntries(this->GetSelectedPoint());
        }
        this->InvokeFunctionChangedCommand();
      }
    } else if ( strcmp(value, ks_("Color Space|HSV (2)") ) == 0) {
      if ( this->RGBATransferFunction->GetColorSpace() != VTK_CTF_HSV ||
           this->RGBATransferFunction->GetHSVWrap() ) {
        this->RGBATransferFunction->SetColorSpace( VTK_CTF_HSV );
        this->RGBATransferFunction->HSVWrapOff();
        this->Update();
        if (this->HasSelection()) {
          this->UpdatePointEntries(this->GetSelectedPoint());
        }
        this->InvokeFunctionChangedCommand();
      }
    }
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::ValueEntriesCallback(const char *) {
  if (!this->HasSelection()) {
    return;
  }

  double parameter;
  if (!this->GetFunctionPointParameter(this->GetSelectedPoint(), &parameter)) {
    return;
  }

  // Get the values from the entries

  double values[VTK_KW_CTFE_NB_ENTRIES];

  int i;
  for (i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
    if (!this->ValueEntries[i]) {
      return;
    }
    values[i] = this->ValueEntries[i]->GetWidget()->GetValueAsDouble();
  }

  // Move the point, check if something has really been moved

  unsigned long mtime = this->GetFunctionMTime();

  this->MoveFunctionPointInColorSpace(
    this->GetSelectedPoint(),
    parameter, values, this->RGBATransferFunction->GetColorSpace());

  if (this->GetFunctionMTime() > mtime) {
    this->InvokePointChangedCommand(this->GetSelectedPoint());
    this->InvokeFunctionChangedCommand();
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::DoubleClickOnPointCallback(
  int x, int y) {
  this->Superclass::DoubleClickOnPointCallback(x, y);

  int id, c_x, c_y;

  // No point found

  if (!this->FindFunctionPointAtCanvasCoordinates(x, y, &id, &c_x, &c_y)) {
    return;
  }

  // Select the point and change its color

  this->SelectPoint(id);

  double rgb[3];
  if (!this->FunctionPointValueIsLocked(id) &&
      this->GetPointColorAsRGB(id, rgb) &&
      vtkKWTkUtilities::QueryUserForColor(
        this->GetApplication(),
        this,
        NULL,
        rgb[0], rgb[1], rgb[2],
        &rgb[0], &rgb[1], &rgb[2])) {
    unsigned long mtime = this->GetFunctionMTime();

    this->SetPointColorAsRGB(id, rgb);

    if (this->GetFunctionMTime() > mtime) {
      this->InvokeFunctionChangedCommand();
    }
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetColorSpaceOptionMenuVisibility(int arg) {
  if (this->ColorSpaceOptionMenuVisibility == arg) {
    return;
  }

  this->ColorSpaceOptionMenuVisibility = arg;

  // Make sure that if the button has to be shown, we create it on the fly if
  // needed

  if (this->ColorSpaceOptionMenuVisibility && this->IsCreated()) {
    this->CreateColorSpaceOptionMenu();
  }

  this->UpdateColorSpaceOptionMenu();

  this->Modified();

  this->Pack();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetColorRampVisibility(int arg) {
  if (this->ColorRampVisibility == arg) {
    return;
  }

  this->ColorRampVisibility = arg;

  // Make sure that if the ramp has to be shown, we create it on the fly if
  // needed.

  if (this->ColorRampVisibility) {
    if (this->IsCreated() && !this->ColorRamp->IsCreated()) {
      this->CreateColorRamp();
    }
  }

  this->RedrawColorRamp(); // if we are hidding it, need to update in canvas

  this->Pack();

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetColorRampPosition(int arg) {
  if (arg < vtkKWRGBATransferFunctionEditor::ColorRampPositionDefault) {
    arg = vtkKWRGBATransferFunctionEditor::ColorRampPositionDefault;
  } else if (arg >
             vtkKWRGBATransferFunctionEditor::ColorRampPositionCanvas) {
    arg = vtkKWRGBATransferFunctionEditor::ColorRampPositionCanvas;
  }

  if (this->ColorRampPosition == arg) {
    return;
  }

  // If the ramp was drawn in the canvas before, make sure we remove it now

  if (this->ColorRampPosition ==
      vtkKWRGBATransferFunctionEditor::ColorRampPositionCanvas) {
    this->CanvasRemoveTag(VTK_KW_CTFE_COLOR_RAMP_TAG);
  }

  this->ColorRampPosition = arg;

  this->Modified();

  this->RedrawColorRamp();

  this->Pack();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetColorRampOutlineStyle(int arg) {
  if (arg < vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleNone) {
    arg = vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleNone;
  } else if (arg >
             vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleSunken) {
    arg = vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleSunken;
  }

  if (this->ColorRampOutlineStyle == arg) {
    return;
  }

  this->ColorRampOutlineStyle = arg;

  this->Modified();

  this->RedrawColorRamp();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetColorRampHeight(int arg) {
  if (this->ColorRampHeight == arg ||
      arg < VTK_KW_CTFE_COLOR_RAMP_HEIGHT_MIN) {
    return;
  }

  this->ColorRampHeight = arg;

  this->RedrawColorRamp();

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::SetValueEntriesVisibility(int arg) {
  if (this->ValueEntriesVisibility == arg) {
    return;
  }

  this->ValueEntriesVisibility = arg;

  // Make sure that if the entries have to be shown, we create it on the fly if
  // needed

  if (this->ValueEntriesVisibility &&
      this->PointEntriesVisibility &&
      this->IsCreated()) {
    this->CreateValueEntries();
  }

  this->UpdatePointEntriesLabel();
  this->UpdatePointEntries(this->GetSelectedPoint());

  this->Modified();

  this->Pack();
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::GetPointColorAsRGB(int id, double rgb[3]) {
  if (!this->HasFunction() || id < 0 || id >= this->GetFunctionSize()) {
    return 0;
  }

  double parameter;
  if (!this->GetFunctionPointParameter(id, &parameter)) {
    return 0;
  }

  this->RGBATransferFunction->GetColor(parameter, rgb);

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::SetPointColorAsRGB(
  int id, const double rgb[3]) {
  double parameter;
  if (!this->GetFunctionPointParameter(id, &parameter)) {
    return 0;
  }

  return this->MoveFunctionPointInColorSpace(id, parameter, rgb, VTK_CTF_RGB);
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::SetPointColorAsRGB(
  int id, double r, double g, double b) {
  double rgb[3];
  rgb[0] = r;
  rgb[1] = g;
  rgb[2] = b;
  return this->SetPointColorAsRGB(id, rgb);
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::GetPointColorAsHSV(int id, double hsv[3]) {
  double rgb[3];
  if (!this->GetPointColorAsRGB(id, rgb)) {
    return 0;
  }

  vtkMath::RGBToHSV(rgb, hsv);

  return 1;
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::SetPointColorAsHSV(
  int id, const double hsv[3]) {
  double parameter;
  if (!this->GetFunctionPointParameter(id, &parameter)) {
    return 0;
  }

  return this->MoveFunctionPointInColorSpace(id, parameter, hsv, VTK_CTF_HSV);
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::SetPointColorAsHSV(
  int id, double h, double s, double v) {
  double hsv[3];
  hsv[0] = h;
  hsv[1] = s;
  hsv[2] = v;
  return this->SetPointColorAsHSV(id, hsv);
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::Redraw() {
  this->Superclass::Redraw();

  // In any cases, check if we have to redraw the color ramp
  // Since the color ramp uses a color function that might not
  // be the color function being edited, we need to check.

  if (!this->IsColorRampUpToDate()) {
    this->RedrawColorRamp();
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::RedrawSizeDependentElements() {
  this->Superclass::RedrawSizeDependentElements();

  this->RedrawColorRamp();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::RedrawPanOnlyDependentElements() {
  this->Superclass::RedrawPanOnlyDependentElements();

  this->RedrawColorRamp();
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::RedrawFunctionDependentElements() {
  this->Superclass::RedrawFunctionDependentElements();

  // This method is called each time the color tfunc has changed
  // but since the histogram depends on the point too if we color by values
  // then update the histogram

  if (this->Histogram && this->ComputeHistogramColorFromValue) {
    this->RedrawHistogram();
  }

  // The color ramp may (or may not) have to be redrawn, depending on which
  // color tfunc is used in the ramp, so it is also catched by Redraw()
  // and in this method (which can be called independently).

  if (!this->IsColorRampUpToDate()) {
    this->RedrawColorRamp();
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::RedrawSinglePointDependentElements(
  int id) {
  this->Superclass::RedrawSinglePointDependentElements(id);

  // The histogram depends on the point too if we color by values

  if (this->Histogram && this->ComputeHistogramColorFromValue) {
    this->RedrawHistogram();
  }

  if (!this->IsColorRampUpToDate()) {
    this->RedrawColorRamp();
  }
}

//----------------------------------------------------------------------------
int vtkKWRGBATransferFunctionEditor::IsColorRampUpToDate() {
  // Which function to use ?

  vtkRGBATransferFunction *func =
    this->ColorRampTransferFunction ?
    this->ColorRampTransferFunction : this->RGBATransferFunction;

  return (func &&
          this->ColorRampVisibility &&
          this->LastRedrawColorRampTime < func->GetMTime()) ? 0 : 1;
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::RedrawColorRamp() {
  if (!this->ColorRamp->IsCreated() ||
      !this->GetFunctionSize() ||
      this->DisableRedraw) {
    return;
  }

  double p_v_range_ext[2];

  if (this->ColorRampVisibility) {
    // Which function to use ?

    vtkRGBATransferFunction *func =
      this->ColorRampTransferFunction ?
      this->ColorRampTransferFunction : this->RGBATransferFunction;

    this->LastRedrawColorRampTime = func->GetMTime();

    int bounds[2], margins[2];
    this->GetCanvasHorizontalSlidingBounds(p_v_range_ext, bounds, margins);

    int in_canvas =
      (this->ColorRampPosition ==
       vtkKWRGBATransferFunctionEditor::ColorRampPositionCanvas);

    // Create the image buffer

    int img_width = bounds[1] - bounds[0] + 1;
    int img_height = this->ColorRampHeight;
    int img_offset_x = 0;

    int i;
    unsigned char *img_ptr;

    // Allocate the image
    // If ramp below the canvas as a label, we have to manually create a margin
    // on the left of the image to align it properly, as this can not
    // be done properly with -padx on the label

    int table_width = img_width;
    if (!in_canvas) {
      img_width += margins[0];
      img_offset_x = margins[0];
    }

    unsigned char *img_buffer = new unsigned char [img_width * img_height * 3];

    // Get the LUT for the parameter range and copy it in the first row

    double *table = new double[table_width * 3];
    func->GetTable(p_v_range_ext[0], p_v_range_ext[1], table_width, table);

    double *table_ptr = table;
    img_ptr = img_buffer + img_offset_x * 3;
    for (i = 0; i < table_width * 3; i++) {
      *img_ptr++ = (unsigned char)(255.0 * *table_ptr++);
    }

    // If ramp below the canvas as a label, fill the margin with
    // background color on the first row

    if (!in_canvas) {
      double bg_r, bg_g, bg_b;
      this->ColorRamp->GetBackgroundColor(&bg_r, &bg_g, &bg_b);
      unsigned char rgb[3];
      rgb[0] = (unsigned char)(bg_r * 255.0);
      rgb[1] = (unsigned char)(bg_g * 255.0);
      rgb[2] = (unsigned char)(bg_b * 255.0);
      img_ptr = img_buffer;
      for (i = 0; i < img_offset_x; i++) {
        *img_ptr++ = rgb[0];
        *img_ptr++ = rgb[1];
        *img_ptr++ = rgb[2];
      }
    }

    // Insert the outline border on that first row if needed

    unsigned char bg_rgb[3], ds_rgb[3], ls_rgb[3], hl_rgb[3];

    if (this->ColorRampOutlineStyle ==
        vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleSolid) {
      img_ptr = img_buffer + img_offset_x * 3;
      *img_ptr++ = 0;
      *img_ptr++ = 0;
      *img_ptr++ = 0;
      img_ptr = img_buffer + (img_width - 1) * 3;
      *img_ptr++ = 0;
      *img_ptr++ = 0;
      *img_ptr++ = 0;
    } else if (this->ColorRampOutlineStyle ==
               vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleSunken) {
      /*
         DDDDDDDDDDDDDDDDH <- B
         DLLLLLLLLLLLLLLBH <- C
         DL.............BH
         DL.............BH <- A
         DBBBBBBBBBBBBBBBH <- D
         HHHHHHHHHHHHHHHHH <- E
      */
      // Sunken Outline: Part A
      this->GetColorRampOutlineSunkenColors(bg_rgb, ds_rgb, ls_rgb, hl_rgb);
      img_ptr = img_buffer + img_offset_x * 3;
      *img_ptr++ = ds_rgb[0];
      *img_ptr++ = ds_rgb[1];
      *img_ptr++ = ds_rgb[2];
      *img_ptr++ = ls_rgb[0];
      *img_ptr++ = ls_rgb[1];
      *img_ptr++ = ls_rgb[2];
      img_ptr = img_buffer + (img_width - 2) * 3;
      *img_ptr++ = bg_rgb[0];
      *img_ptr++ = bg_rgb[1];
      *img_ptr++ = bg_rgb[2];
      *img_ptr++ = hl_rgb[0];
      *img_ptr++ = hl_rgb[1];
      *img_ptr++ = hl_rgb[2];
    }

    // Replicate the first row to all other rows

    img_ptr = img_buffer + img_width * 3;
    for (i = 1; i < img_height; i++) {
      memmove(img_ptr, img_buffer, img_width * 3);
      img_ptr += img_width * 3;
    }

    // Complete the outline top/bottom

    if (this->ColorRampOutlineStyle ==
        vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleSolid) {
      memset(img_buffer + img_offset_x * 3, 0, table_width * 3);
      memset(img_buffer + (img_width * (img_height - 1) + img_offset_x) * 3,
             0, table_width * 3);
    } else if (this->ColorRampOutlineStyle ==
               vtkKWRGBATransferFunctionEditor::ColorRampOutlineStyleSunken) {
      // Sunken Outline: Part B
      img_ptr = img_buffer + img_offset_x * 3;
      for (i = 0; i < table_width - 1; i++) {
        *img_ptr++ = ds_rgb[0];
        *img_ptr++ = ds_rgb[1];
        *img_ptr++ = ds_rgb[2];
      }
      *img_ptr++ = hl_rgb[0];
      *img_ptr++ = hl_rgb[1];
      *img_ptr++ = hl_rgb[2];

      // Sunken Outline: Part C
      img_ptr = img_buffer + (img_width + img_offset_x + 1) * 3;
      for (i = 0; i < table_width - 3; i++) {
        *img_ptr++ = ls_rgb[0];
        *img_ptr++ = ls_rgb[1];
        *img_ptr++ = ls_rgb[2];
      }
      *img_ptr++ = bg_rgb[0];
      *img_ptr++ = bg_rgb[1];
      *img_ptr++ = bg_rgb[2];

      // Sunken Outline: Part D
      img_ptr = img_buffer + (img_width * (img_height - 2) + img_offset_x+1)*3;
      for (i = 0; i < table_width - 2; i++) {
        *img_ptr++ = bg_rgb[0];
        *img_ptr++ = bg_rgb[1];
        *img_ptr++ = bg_rgb[2];
      }

      // Sunken Outline: Part E
      img_ptr = img_buffer + (img_width * (img_height - 1) + img_offset_x) * 3;
      for (i = 0; i < table_width; i++) {
        *img_ptr++ = hl_rgb[0];
        *img_ptr++ = hl_rgb[1];
        *img_ptr++ = hl_rgb[2];
      }
    }

    // Update the image

    this->ColorRamp->SetImageToPixels(
      img_buffer, img_width, img_height, 3);

    delete [] img_buffer;
    delete [] table;
  }

  // If the ramp has to be in the canvas, draw it now or remove it

  if (this->ColorRampPosition ==
      vtkKWRGBATransferFunctionEditor::ColorRampPositionCanvas &&
      this->Canvas && this->Canvas->IsAlive()) {
    const char *canv = this->Canvas->GetWidgetName();
    ostrstream tk_cmd;

    // Create/remove the image item in the canvas only when needed

    int has_tag = this->CanvasHasTag(VTK_KW_CTFE_COLOR_RAMP_TAG);
    if (!has_tag) {
      if (this->ColorRampVisibility) {
        vtksys_stl::string image_name(
          this->ColorRamp->GetConfigurationOption("-image"));
        tk_cmd << canv << " create image 0 0 -anchor nw "
        << " -image " << image_name.c_str()
        << " -tags {" << VTK_KW_CTFE_COLOR_RAMP_TAG << "}" << endl;
        tk_cmd << canv << " lower " << VTK_KW_CTFE_COLOR_RAMP_TAG
        << " {" << vtkKWParameterValueFunctionEditor::FunctionTag
        << "||" << vtkKWParameterValueFunctionEditor::FrameForegroundTag
        << "||" << vtkKWParameterValueFunctionEditor::HistogramTag
        << "||" << vtkKWParameterValueFunctionEditor::SecondaryHistogramTag
        << "}" << endl;
      }
    } else {
      if (!this->ColorRampVisibility) {
        tk_cmd << canv << " delete " << VTK_KW_CTFE_COLOR_RAMP_TAG << endl;
      }
    }

    // Update coordinates

    if (this->ColorRampVisibility) {
      double factors[2] = {0.0, 0.0};
      this->GetCanvasScalingFactors(factors);

      double *v_v_range = this->GetVisibleValueRange();
      double *v_w_range = this->GetWholeValueRange();

      double c_y = ceil(
                     (v_w_range[1] - (v_v_range[1] + v_v_range[0]) * 0.5) * factors[1]
                     - (double)this->ColorRampHeight * 0.5);

      tk_cmd << canv << " coords " << VTK_KW_CTFE_COLOR_RAMP_TAG
      << " " << p_v_range_ext[0] * factors[0] << " " << c_y << endl;
    }

    tk_cmd << ends;
    this->Script(tk_cmd.str());
    tk_cmd.rdbuf()->freeze(0);
  }
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::RedrawHistogram() {
  if (!this->IsCreated() || !this->Canvas || !this->Canvas->IsAlive() ||
      this->DisableRedraw) {
    return;
  }

  const char *canv = this->Canvas->GetWidgetName();

  int has_hist_tag = 0, has_secondary_hist_tag = 0;
  if (this->ColorRampPosition ==
      vtkKWRGBATransferFunctionEditor::ColorRampPositionCanvas) {
    has_hist_tag =
      this->CanvasHasTag(vtkKWParameterValueFunctionEditor::HistogramTag);
    has_secondary_hist_tag =
      this->CanvasHasTag(vtkKWParameterValueFunctionEditor::SecondaryHistogramTag);
  }

  this->Superclass::RedrawHistogram();

  if (this->ColorRampPosition !=
      vtkKWRGBATransferFunctionEditor::ColorRampPositionCanvas) {
    return;
  }

  ostrstream tk_cmd;

  // If the primary histogram has just been created, raise or lower it

  if (!has_hist_tag && has_hist_tag !=
      this->CanvasHasTag(vtkKWParameterValueFunctionEditor::HistogramTag)) {
    tk_cmd << canv << " raise "
    << vtkKWParameterValueFunctionEditor::HistogramTag
    << " " << VTK_KW_CTFE_COLOR_RAMP_TAG << endl;
  }

  // If the secondary histogram has just been created, raise or lower it

  if (!has_secondary_hist_tag && has_secondary_hist_tag !=
      this->CanvasHasTag(vtkKWParameterValueFunctionEditor::SecondaryHistogramTag)) {
    tk_cmd << canv << " raise "
    << vtkKWParameterValueFunctionEditor::SecondaryHistogramTag
    << " " << VTK_KW_CTFE_COLOR_RAMP_TAG << endl;
  }

  tk_cmd << ends;
  this->Script(tk_cmd.str());
  tk_cmd.rdbuf()->freeze(0);
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::GetColorRampOutlineSunkenColors(
  unsigned char bg_rgb[3],
  unsigned char ds_rgb[3],
  unsigned char ls_rgb[3],
  unsigned char hl_rgb[3]) {
  if (!this->ColorRamp || !this->ColorRamp->IsCreated()) {
    return;
  }

  double fr, fg, fb;
  double fh, fs, fv;

  this->ColorRamp->GetBackgroundColor(&fr, &fg, &fb);

  bg_rgb[0] = (unsigned char)(fr * 255.0);
  bg_rgb[1] = (unsigned char)(fg * 255.0);
  bg_rgb[2] = (unsigned char)(fb * 255.0);

  if (fr == fg && fg == fb) {
    fh = fs = 0.0;
    fv = fr;
  } else {
    vtkMath::RGBToHSV(fr, fg, fb, &fh, &fs, &fv);
  }

  vtkMath::HSVToRGB(fh, fs, fv * 0.3, &fr, &fg, &fb);
  ds_rgb[0] = (unsigned char)(fr * 255.0);
  ds_rgb[1] = (unsigned char)(fg * 255.0);
  ds_rgb[2] = (unsigned char)(fb * 255.0);

  vtkMath::HSVToRGB(fh, fs, fv * 0.6, &fr, &fg, &fb);
  ls_rgb[0] = (unsigned char)(fr * 255.0);
  ls_rgb[1] = (unsigned char)(fg * 255.0);
  ls_rgb[2] = (unsigned char)(fb * 255.0);

  vtkMath::HSVToRGB(fh, fs, 1.0, &fr, &fg, &fb);
  hl_rgb[0] = (unsigned char)(fr * 255.0);
  hl_rgb[1] = (unsigned char)(fg * 255.0);
  hl_rgb[2] = (unsigned char)(fb * 255.0);
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::UpdateHistogramImageDescriptor(
  vtkKWHistogram::ImageDescriptor *desc) {
  this->Superclass::UpdateHistogramImageDescriptor(desc);

#if 0
  if (this->ComputeHistogramColorFromValue) {
    desc->ColorTransferFunction =
      this->ColorRampTransferFunction
      ? this->ColorRampTransferFunction : this->RGBATransferFunction;
    desc->DrawGrid = 1;
  }
#endif
}

//----------------------------------------------------------------------------
void vtkKWRGBATransferFunctionEditor::PrintSelf(ostream& os, vtkIndent indent) {
  this->Superclass::PrintSelf(os,indent);

  os << indent << "ValueEntriesVisibility: "
  << (this->ValueEntriesVisibility ? "On" : "Off") << endl;

  os << indent << "ColorSpaceOptionMenuVisibility: "
  << (this->ColorSpaceOptionMenuVisibility ? "On" : "Off") << endl;

  os << indent << "ColorRampVisibility: "
  << (this->ColorRampVisibility ? "On" : "Off") << endl;

  os << indent << "ColorRampHeight: " << this->ColorRampHeight << endl;
  os << indent << "ColorRampPosition: " << this->ColorRampPosition << endl;
  os << indent << "ColorRampOutlineStyle: " << this->ColorRampOutlineStyle << endl;

  os << indent << "RGBATransferFunction: ";
  if (this->RGBATransferFunction) {
    os << endl;
    this->RGBATransferFunction->PrintSelf(os, indent.GetNextIndent());
  } else {
    os << "None" << endl;
  }

  os << indent << "ColorRampTransferFunction: ";
  if (this->ColorRampTransferFunction) {
    os << endl;
    this->ColorRampTransferFunction->PrintSelf(os, indent.GetNextIndent());
  } else {
    os << "None" << endl;
  }

  os << indent << "ColorSpaceOptionMenu: ";
  if (this->ColorSpaceOptionMenu) {
    os << endl;
    this->ColorSpaceOptionMenu->PrintSelf(os, indent.GetNextIndent());
  } else {
    os << "None" << endl;
  }

  int i;
  for (i = 0; i < VTK_KW_CTFE_NB_ENTRIES; i++) {
    os << indent << "ValueEntries[" << i << "]: ";
    if (this->ValueEntries[i]) {
      os << endl;
      this->ValueEntries[i]->PrintSelf(os, indent.GetNextIndent());
    } else {
      os << "None" << endl;
    }
  }
}

