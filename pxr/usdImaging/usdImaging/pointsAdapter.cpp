//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/usdImaging/usdImaging/pointsAdapter.h"

#include "pxr/usdImaging/usdImaging/dataSourcePoints.h"
#include "pxr/usdImaging/usdImaging/delegate.h"
#include "pxr/usdImaging/usdImaging/indexProxy.h"
#include "pxr/usdImaging/usdImaging/tokens.h"

#include "pxr/imaging/hd/points.h"
#include "pxr/imaging/hd/perfLog.h"

#include "pxr/usd/usdGeom/points.h"
#include "pxr/usd/usdGeom/primvarsAPI.h"

#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_REGISTRY_FUNCTION(TfType)
{
    typedef UsdImagingPointsAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

UsdImagingPointsAdapter::~UsdImagingPointsAdapter() 
{
}

TfTokenVector
UsdImagingPointsAdapter::GetImagingSubprims()
{
    return { TfToken() };
}

TfToken
UsdImagingPointsAdapter::GetImagingSubprimType(TfToken const& subprim)
{
    if (subprim.IsEmpty()) {
        return HdPrimTypeTokens->points;
    }
    return TfToken();
}

HdContainerDataSourceHandle
UsdImagingPointsAdapter::GetImagingSubprimData(
        TfToken const& subprim,
        UsdPrim const& prim,
        const UsdImagingDataSourceStageGlobals &stageGlobals)
{
    if (subprim.IsEmpty()) {
        return UsdImagingDataSourcePointsPrim::New(
            prim.GetPath(),
            prim,
            stageGlobals);
    }
    return nullptr;
}

bool
UsdImagingPointsAdapter::IsSupported(UsdImagingIndexProxy const* index) const
{
    return index->IsRprimTypeSupported(HdPrimTypeTokens->points);
}

SdfPath
UsdImagingPointsAdapter::Populate(UsdPrim const& prim, 
                            UsdImagingIndexProxy* index,
                            UsdImagingInstancerContext const* instancerContext)
{
    return _AddRprim(HdPrimTypeTokens->points,
                     prim, index, GetMaterialUsdPath(prim), instancerContext);
}

void 
UsdImagingPointsAdapter::TrackVariability(UsdPrim const& prim,
                                          SdfPath const& cachePath,
                                          HdDirtyBits* timeVaryingBits,
                                          UsdImagingInstancerContext const* 
                                              instancerContext) const
{
    BaseAdapter::TrackVariability(
        prim, cachePath, timeVaryingBits, instancerContext);

    // Discover time-varying points.
    _IsVarying(prim,
               UsdGeomTokens->points,
               HdChangeTracker::DirtyPoints,
               UsdImagingTokens->usdVaryingPrimvar,
               timeVaryingBits,
               /*isInherited*/false);

    // Check for time-varying primvars:widths, and if that attribute
    // doesn't exist also check for time-varying widths.
    bool widthsExists = false;
    _IsVarying(prim,
               UsdImagingTokens->primvarsWidths,
               HdChangeTracker::DirtyWidths,
               UsdImagingTokens->usdVaryingWidths,
               timeVaryingBits,
               /*isInherited*/false,
               &widthsExists);
    if (!widthsExists) {
        UsdGeomPrimvar pv = _GetInheritedPrimvar(prim, HdTokens->widths);
        if (pv && pv.ValueMightBeTimeVarying()) {
            *timeVaryingBits |= HdChangeTracker::DirtyWidths;
            HD_PERF_COUNTER_INCR(UsdImagingTokens->usdVaryingWidths);
            widthsExists = true;
        }
    }
    if (!widthsExists) {
        _IsVarying(prim, UsdGeomTokens->widths,
                HdChangeTracker::DirtyWidths,
                UsdImagingTokens->usdVaryingWidths,
                timeVaryingBits,
                /*isInherited*/false);
    }

    // Check for time-varying primvars:normals, and if that attribute
    // doesn't exist also check for time-varying normals.
    bool normalsExists = false;
    _IsVarying(prim,
               UsdImagingTokens->primvarsNormals,
               HdChangeTracker::DirtyNormals,
               UsdImagingTokens->usdVaryingNormals,
               timeVaryingBits,
               /*isInherited*/false,
               &normalsExists);
    if (!normalsExists) {
        UsdGeomPrimvar pv = _GetInheritedPrimvar(prim, HdTokens->normals);
        if (pv && pv.ValueMightBeTimeVarying()) {
            *timeVaryingBits |= HdChangeTracker::DirtyNormals;
            HD_PERF_COUNTER_INCR(UsdImagingTokens->usdVaryingNormals);
            normalsExists = true;
        }
    }
    if (!normalsExists) {
        _IsVarying(prim, UsdGeomTokens->normals,
                HdChangeTracker::DirtyNormals,
                UsdImagingTokens->usdVaryingNormals,
                timeVaryingBits,
                /*isInherited*/false);
    }
}

bool
UsdImagingPointsAdapter::_IsBuiltinPrimvar(TfToken const& primvarName) const
{
    return (primvarName == HdTokens->normals ||
            primvarName == HdTokens->widths) ||
        UsdImagingGprimAdapter::_IsBuiltinPrimvar(primvarName);
}

void 
UsdImagingPointsAdapter::UpdateForTime(UsdPrim const& prim,
                                       SdfPath const& cachePath, 
                                       UsdTimeCode time,
                                       HdDirtyBits requestedBits,
                                       UsdImagingInstancerContext const* 
                                           instancerContext) const
{
    BaseAdapter::UpdateForTime(
        prim, cachePath, time, requestedBits, instancerContext);

    UsdImagingPrimvarDescCache* primvarDescCache = _GetPrimvarDescCache();
    HdPrimvarDescriptorVector& primvars = 
        primvarDescCache->GetPrimvars(cachePath);

    if (requestedBits & HdChangeTracker::DirtyWidths) {
        // First check for "primvars:widths"
        UsdGeomPrimvarsAPI primvarsApi(prim);
        UsdGeomPrimvar pv = primvarsApi.GetPrimvar(
            UsdImagingTokens->primvarsWidths);
        if (!pv) {
            // If it's not found locally, see if it's inherited
            pv = _GetInheritedPrimvar(prim, HdTokens->widths);
        }

        if (pv) {
            _ComputeAndMergePrimvar(prim, pv, time, &primvars);
        } else {
            UsdGeomPoints points(prim);
            VtFloatArray widths;
            if (points.GetWidthsAttr().Get(&widths, time)) {
                HdInterpolation interpolation = _UsdToHdInterpolation(
                    points.GetWidthsInterpolation());
                _MergePrimvar(&primvars, UsdGeomTokens->widths, interpolation);
            } else {
                _RemovePrimvar(&primvars, UsdGeomTokens->widths);
            }
        }
    }

    if (requestedBits & HdChangeTracker::DirtyNormals) {
        // First check for "primvars:normals"
        UsdGeomPrimvarsAPI primvarsApi(prim);
        UsdGeomPrimvar pv = primvarsApi.GetPrimvar(
            UsdImagingTokens->primvarsNormals);
        if (!pv) {
            // If it's not found locally, see if it's inherited
            pv = _GetInheritedPrimvar(prim, HdTokens->normals);
        }
    
        if (pv) {
            _ComputeAndMergePrimvar(prim, pv, time, &primvars);
        } else {
            UsdGeomPoints points(prim);
            VtVec3fArray normals;
            if (points.GetNormalsAttr().Get(&normals, time)) {
                _MergePrimvar(
                    &primvars,
                    UsdGeomTokens->normals,
                    _UsdToHdInterpolation(points.GetNormalsInterpolation()),
                    HdPrimvarRoleTokens->normal);
            } else {
                _RemovePrimvar(&primvars, UsdGeomTokens->normals);
            }
        }
    }
}

HdDirtyBits
UsdImagingPointsAdapter::ProcessPropertyChange(UsdPrim const& prim,
                                               SdfPath const& cachePath,
                                               TfToken const& propertyName)
{
    if (propertyName == UsdGeomTokens->points)
        return HdChangeTracker::DirtyPoints;

    // Handle attributes that are treated as "built-in" primvars.
    if (propertyName == UsdGeomTokens->widths) {
        UsdGeomPoints points(prim);
        return UsdImagingPrimAdapter::_ProcessNonPrefixedPrimvarPropertyChange(
            prim, cachePath, propertyName, HdTokens->widths,
            _UsdToHdInterpolation(points.GetWidthsInterpolation()),
            HdChangeTracker::DirtyWidths);
    
    } else if (propertyName == UsdGeomTokens->normals) {
        UsdGeomPoints points(prim);
        return UsdImagingPrimAdapter::_ProcessNonPrefixedPrimvarPropertyChange(
            prim, cachePath, propertyName, HdTokens->normals,
            _UsdToHdInterpolation(points.GetNormalsInterpolation()),
            HdChangeTracker::DirtyNormals);
    }
    // Handle prefixed primvars that use special dirty bits.
    else if (propertyName == UsdImagingTokens->primvarsWidths) {
        return UsdImagingPrimAdapter::_ProcessPrefixedPrimvarPropertyChange(
            prim, cachePath, propertyName, HdChangeTracker::DirtyWidths);
    
    } else if (propertyName == UsdImagingTokens->primvarsNormals) {
        return UsdImagingPrimAdapter::_ProcessPrefixedPrimvarPropertyChange(
                prim, cachePath, propertyName, HdChangeTracker::DirtyNormals);
    }

    // Allow base class to handle change processing.
    return BaseAdapter::ProcessPropertyChange(prim, cachePath, propertyName);
}

/*virtual*/
VtValue
UsdImagingPointsAdapter::Get(UsdPrim const& prim,
                             SdfPath const& cachePath,
                             TfToken const& key,
                             UsdTimeCode time,
                             VtIntArray *outIndices) const
{
    TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (key == HdTokens->normals) {
        // First check for "primvars:normals"
        UsdGeomPrimvarsAPI primvarsApi(prim);
        UsdGeomPrimvar pv = primvarsApi.GetPrimvar(
            UsdImagingTokens->primvarsNormals);
        if (!pv) {
            // If it's not found locally, see if it's inherited
            pv = _GetInheritedPrimvar(prim, HdTokens->normals);
        }

        VtValue value;
        
        if (outIndices) {
            if (pv && pv.Get(&value, time)) {
                pv.GetIndices(outIndices, time);
                return value;
            }
        } else if (pv && pv.ComputeFlattened(&value, time)) {
            return value;
        }

        // If there's no "primvars:normals",
        // fall back to UsdGeomPoints's "normals" attribute. 
        UsdGeomPoints points(prim);
        VtVec3fArray normals;
        if (points && points.GetNormalsAttr().Get(&normals, time)) {
            value = normals;
            return value;
        }

    } else if (key == HdTokens->widths) {
        // First check for "primvars:widths"
        UsdGeomPrimvarsAPI primvarsApi(prim);
        UsdGeomPrimvar pv = primvarsApi.GetPrimvar(
            UsdImagingTokens->primvarsWidths);
        if (!pv) {
            // If it's not found locally, see if it's inherited
            pv = _GetInheritedPrimvar(prim, HdTokens->widths);
        }

        VtValue value;

        if (outIndices) {
            if (pv && pv.Get(&value, time)) {
                pv.GetIndices(outIndices, time);
                return value;
            }
        } else if (pv && pv.ComputeFlattened(&value, time)) {
            return value;
        }

        // Fallback to UsdGeomPoints' "normals" attribute.
        UsdGeomPoints points(prim);
        VtFloatArray widths;
        if (points && points.GetWidthsAttr().Get(&widths, time)) {
            value = widths;
            return value;
        }
    }

    return BaseAdapter::Get(prim, cachePath, key, time, outIndices);
}

PXR_NAMESPACE_CLOSE_SCOPE

