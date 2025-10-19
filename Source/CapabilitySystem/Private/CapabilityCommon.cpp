#include "CapabilitySystem/Public/CapabilityCommon.h"
#include "CapabilitySystem/Public/CapabilityMetaHead.h"

DEFINE_LOG_CATEGORY(CapabilitySystemLog);

void FCapabilityObjectRefSet::CallBeginPlay() {
    for (auto Ref : ObjectRefs)
        if (Ref) Ref->NativeBeginPlay();
}

void FCapabilityObjectRefSet::CallPreEndPlay() {
    for (int i = ObjectRefs.Num() - 1; i >= 0; --i) {
        if (ObjectRefs[i])
            ObjectRefs[i]->NativePreEndPlay();
    }
}

void FCapabilityObjectRefSet::CallEndPlay() {
    for (int i = ObjectRefs.Num() - 1; i >= 0; --i) {
        if (ObjectRefs[i])
            ObjectRefs[i]->NativeEndPlay();
    }
}

void FCapabilityObjectRefSet::MarkGC() {
    for (int i = ObjectRefs.Num() - 1; i >= 0; --i) {
        if (ObjectRefs[i])
            ObjectRefs[i]->MarkAsGarbage();
    }
    if (MetaHead) { MetaHead->MarkAsGarbage(); }
}