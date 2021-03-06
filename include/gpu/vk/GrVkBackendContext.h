/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrVkBackendContext_DEFINED
#define GrVkBackendContext_DEFINED

#include "GrVkTypes.h"
#include "SkRefCnt.h"
#include "vk/GrVkMemoryAllocator.h"

class GrVkExtensions;

enum GrVkExtensionFlags {
    kEXT_debug_report_GrVkExtensionFlag    = 0x0001,
    kNV_glsl_shader_GrVkExtensionFlag      = 0x0002,
    kKHR_surface_GrVkExtensionFlag         = 0x0004,
    kKHR_swapchain_GrVkExtensionFlag       = 0x0008,
    kKHR_win32_surface_GrVkExtensionFlag   = 0x0010,
    kKHR_android_surface_GrVkExtensionFlag = 0x0020,
    kKHR_xcb_surface_GrVkExtensionFlag     = 0x0040,
};

enum GrVkFeatureFlags {
    kGeometryShader_GrVkFeatureFlag    = 0x0001,
    kDualSrcBlend_GrVkFeatureFlag      = 0x0002,
    kSampleRateShading_GrVkFeatureFlag = 0x0004,
    kIgnoreAllFlags_GrVkFeatureFlag    = 0x0008,
};

// The BackendContext contains all of the base Vulkan objects needed by the GrVkGpu. The assumption
// is that the client will set these up and pass them to the GrVkGpu constructor. The VkDevice
// created must support at least one graphics queue, which is passed in as well.
// The QueueFamilyIndex must match the family of the given queue. It is needed for CommandPool
// creation, and any GrBackendObjects handed to us (e.g., for wrapped textures) needs to be created
// in or transitioned to that family. The refs held by members of this struct must be released
// (either by deleting the struct or manually releasing the refs) before the underlying vulkan
// device and instance are destroyed.
struct SK_API GrVkBackendContext {
    VkInstance                 fInstance;
    VkPhysicalDevice           fPhysicalDevice;
    VkDevice                   fDevice;
    VkQueue                    fQueue;
    uint32_t                   fGraphicsQueueIndex;
    uint32_t                   fMinAPIVersion; // Deprecated. Set fInstanceVersion instead.
    uint32_t                   fInstanceVersion = 0;
    uint32_t                   fExtensions = 0; // Deprecated. Use fVkExtensions instead.
    const GrVkExtensions*      fVkExtensions = nullptr;
    uint32_t                   fFeatures = kIgnoreAllFlags_GrVkFeatureFlag;
    VkPhysicalDeviceFeatures   fDeviceFeatures;
    sk_sp<GrVkMemoryAllocator> fMemoryAllocator;
    GrVkGetProc                fGetProc = nullptr;
    // This is deprecated and should be set to false. The client is responsible for managing the
    // lifetime of the VkInstance and VkDevice objects.
    bool                       fOwnsInstanceAndDevice = false;
};

#endif
