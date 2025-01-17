//
// Copyright 2022 Pixar
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
#include "pxr/usdImaging/usdProcImaging/generativeProceduralAdapter.h"
#include "pxr/usdImaging/usdImaging/tokens.h"

#include "pxr/usd/usdProc/generativeProcedural.h"

#include "pxr/usdImaging/usdImaging/indexProxy.h"
#include "pxr/base/tf/type.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (inertGenerativeProcedural)
);

TF_REGISTRY_FUNCTION(TfType)
{
    typedef UsdProcImagingGenerativeProceduralAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

SdfPath
UsdProcImagingGenerativeProceduralAdapter::Populate(
    UsdPrim const& prim,
    UsdImagingIndexProxy* index,
    UsdImagingInstancerContext const *instancerContext)
{
    TfToken rprimType;
    UsdProcGenerativeProcedural genProc(prim);

    VtValue procSysValue;

    if (UsdAttribute procSysAttr = genProc.GetProceduralSystemAttr()) {
        procSysAttr.Get(&procSysValue);
    }

    if (procSysValue.IsHolding<TfToken>()) {
        rprimType = procSysValue.UncheckedGet<TfToken>();
    }

    if (rprimType.IsEmpty()) {
        rprimType = _tokens->inertGenerativeProcedural;
    }

    index->InsertRprim(rprimType, prim.GetPath(), prim);

    return prim.GetPath();
}

bool
UsdProcImagingGenerativeProceduralAdapter::IsSupported(
    UsdImagingIndexProxy const* index) const
{
    return true;
}

void
UsdProcImagingGenerativeProceduralAdapter::UpdateForTime(
    UsdPrim const& prim,
    SdfPath const& cachePath, 
    UsdTimeCode time,
    HdDirtyBits requestedBits,
    UsdImagingInstancerContext const* 
        instancerContext) const
{
    UsdImagingPrimvarDescCache* primvarDescCache = _GetPrimvarDescCache();
    HdPrimvarDescriptorVector& vPrimvars = 
        primvarDescCache->GetPrimvars(cachePath);

    if (requestedBits & HdChangeTracker::DirtyPrimvar) {
        std::vector<UsdGeomPrimvar> primvars;

        UsdImaging_InheritedPrimvarStrategy::value_type inheritedPrimvarRecord =
            _GetInheritedPrimvars(prim.GetParent());
        if (inheritedPrimvarRecord) {
            primvars = inheritedPrimvarRecord->primvars;
        }

        UsdGeomPrimvarsAPI primvarsAPI(prim);
        std::vector<UsdGeomPrimvar> local = primvarsAPI.GetPrimvarsWithValues();
        primvars.insert(primvars.end(), local.begin(), local.end());

        for (auto const &pv : primvars) {
            _ComputeAndMergePrimvar(prim, pv, time, &vPrimvars);
        }

        for (UsdProperty prop :
                prim.GetAuthoredPropertiesInNamespace("primvars:")) {
            if (UsdRelationship rel = prop.As<UsdRelationship>()) {
                
                _MergePrimvar(
                    &vPrimvars,
                    rel.GetBaseName(),
                    HdInterpolationConstant,
                    HdPrimvarRoleTokens->none);
            }
        }
    }
}

VtValue
UsdProcImagingGenerativeProceduralAdapter::Get(UsdPrim const& prim,
    SdfPath const& cachePath,
    TfToken const& key,
    UsdTimeCode time,
    VtIntArray *outIndices) const
{
    VtValue value;
    if (UsdGeomPrimvar pv = UsdGeomPrimvarsAPI(prim).GetPrimvar(key)) {
        
        if (outIndices) {
            if (pv && pv.Get(&value, time)) {
                pv.GetIndices(outIndices, time);
                return value;
            }
        } else if (pv && pv.ComputeFlattened(&value, time)) {
            return value;
        }
    } else if (UsdGeomPrimvar pv = _GetInheritedPrimvar(prim, key)) {
        if (outIndices) {
            if (pv && pv.Get(&value, time)) {
                pv.GetIndices(outIndices, time);
                return value;
            }
        } else if (pv && pv.ComputeFlattened(&value, time)) {
            return value;
        }
    }


    // no primvar result? Try for a primvar relationship
    TfToken prefixedName(("primvars:" + key.GetString()).c_str());
    if (UsdRelationship rel = prim.GetRelationship(prefixedName)) {
        SdfPathVector targets;
        rel.GetTargets(&targets);

        return VtValue(VtArray<SdfPath>(targets.begin(), targets.end()));
    }

    return value;
}

HdDirtyBits
UsdProcImagingGenerativeProceduralAdapter::ProcessPropertyChange(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    TfToken const& propertyName)
{
    // if "proceduralSystem" changes, our hydra type will change and we
    // indicate that via AllDirty
    if (propertyName == UsdProcTokens->proceduralSystem) {
        return HdChangeTracker::AllDirty;
    }

    HdDirtyBits result = HdChangeTracker::Clean;

    if (UsdGeomPrimvarsAPI::CanContainPropertyName(propertyName)) {
        result |= HdChangeTracker::DirtyPrimvar;
    }

    return result;
}

void
UsdProcImagingGenerativeProceduralAdapter::_RemovePrim(
    SdfPath const& cachePath,
    UsdImagingIndexProxy* index)
{
    index->RemoveRprim(cachePath);
}

void 
UsdProcImagingGenerativeProceduralAdapter::TrackVariability(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    HdDirtyBits* timeVaryingBits,
    UsdImagingInstancerContext const*  nstancerContext) const
{
    // XXX copied/pared from UsdImagingGprimAdapter
    if (!(*timeVaryingBits & HdChangeTracker::DirtyPrimvar)) {
        // See if any local primvars are time-dependent.
        UsdGeomPrimvarsAPI primvarsAPI(prim);
        std::vector<UsdGeomPrimvar> primvars =
            primvarsAPI.GetPrimvarsWithValues();
        for (UsdGeomPrimvar const& pv : primvars) {
            if (pv.ValueMightBeTimeVarying()) {
                *timeVaryingBits |= HdChangeTracker::DirtyPrimvar;
                HD_PERF_COUNTER_INCR(UsdImagingTokens->usdVaryingPrimvar);
                break;
            }
        }
    }

    // Discover time-varying extent.
    _IsVarying(prim,
               UsdGeomTokens->extent,
               HdChangeTracker::DirtyExtent,
               UsdImagingTokens->usdVaryingExtent,
               timeVaryingBits,
               false);

    // Discover time-varying transforms.
    _IsTransformVarying(prim,
               HdChangeTracker::DirtyTransform,
               UsdImagingTokens->usdVaryingXform,
               timeVaryingBits);

    // Discover time-varying visibility.
    _IsVarying(prim,
               UsdGeomTokens->visibility,
               HdChangeTracker::DirtyVisibility,
               UsdImagingTokens->usdVaryingVisibility,
               timeVaryingBits,
               true);
}

void
UsdProcImagingGenerativeProceduralAdapter::MarkDirty(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    HdDirtyBits dirty,
    UsdImagingIndexProxy* index)
{
    index->MarkRprimDirty(cachePath, dirty);
}

void
UsdProcImagingGenerativeProceduralAdapter::MarkTransformDirty(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    UsdImagingIndexProxy* index)
{
    index->MarkRprimDirty(cachePath, HdChangeTracker::DirtyTransform);
}

void
UsdProcImagingGenerativeProceduralAdapter::MarkVisibilityDirty(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    UsdImagingIndexProxy* index)
{
    index->MarkRprimDirty(cachePath, HdChangeTracker::DirtyVisibility);
}


PXR_NAMESPACE_CLOSE_SCOPE