#ifndef EFI_HH
#define EFI_HH

#include <core/limine.hh>
#include <ktl/string_view>
#include <ktl/slice>
#include <ktl/optional>

#if 0

class Efi {
public:
    using Status        = u64;
    using PhysicalAddr  = u64;
    using VirtualAddr   = u64;
    using Char16        = u16;

    static constexpr u64 EFI_MEMORY_RUNTIME = 0x8000000000000000ULL;

    enum ResetType : u32 {
        EfiResetCold      = 0,
        EfiResetWarm      = 1,
        EfiResetShutdown  = 2,
        EfiResetPlatformSpecific = 3
    };

    struct [[gnu::packed]] Guid {
        u32  Data1;
        u16  Data2;
        u16  Data3;
        u8   Data4[8];
    };

    struct [[gnu::packed]] TableHeader {
        u64  Signature;
        u32  Revision;
        u32  HeaderSize;
        u32  CRC32;
        u32  Reserved;
    };

    struct [[gnu::packed]] Time {
        u16 Year;
        u8  Month;
        u8  Day;
        u8  Hour;
        u8  Minute;
        u8  Second;
        u8  Pad1;
        u32 Nanosecond;
        u16  TimeZone;
        u8  Daylight;
        u8  Pad2;
    };

    struct [[gnu::packed]] TimeCapabilities {
        u32 Resolution;
        u32 Accuracy;
        bool SetsToZero;
    };

    struct RuntimeServices;

    struct [[gnu::packed]] ConfigurationTable {
        Guid   VendorGuid;
        void*  VendorTable;
    };

    struct [[gnu::packed]] SystemTable {
        TableHeader         Hdr;
        Char16*             FirmwareVendor;
        u32              FirmwareRevision;
        void*               ConsoleInHandle;
        void*               ConIn;
        void*               ConsoleOutHandle;
        void*               ConOut;
        void*               StandardErrorHandle;
        void*               StdErr;
        RuntimeServices*    RuntimeServices;
        void*               BootServices;
        uptr               NumberOfTableEntries;
        ConfigurationTable* ConfigurationTable;
    };

    struct [[gnu::packed]] RuntimeServices {
        TableHeader  Hdr;

        Status (*GetTime)(
            Time*               /*Time*/,
            TimeCapabilities*   /*Capabilities*/
        );
        Status (*SetTime)(
            const Time*         /*Time*/
        );
        Status (*GetWakeupTime)(
            bool*               /*Enabled*/,
            bool*               /*Pending*/,
            Time*               /*Time*/
        );
        Status (*SetWakeupTime)(
            bool                /*Enabled*/,
            const Time*         /*Time*/
        );

        Status (*SetVirtualAddressMap)(
            uptr               /*MemoryMapSize*/,
            uptr               /*DescriptorSize*/,
            uptr               /*DescriptorVersion*/,
            void*               /*VirtualMap*/
        );
        Status (*ConvertPointer)(
            uptr               /*DebugDisposition*/,
            void**              /*Address*/
        );

        Status (*GetVariable)(
            Char16*             /*VariableName*/,
            const Guid*         /*VendorGuid*/,
            u32*             /*Attributes*/,
            uptr*              /*DataSize*/,
            void*               /*Data*/
        );
        Status (*GetNextVariableName)(
            uptr*              /*VariableNameSize*/,
            Char16*             /*VariableName*/,
            Guid*               /*VendorGuid*/
        );
        Status (*SetVariable)(
            Char16*             /*VariableName*/,
            const Guid*         /*VendorGuid*/,
            u32              /*Attributes*/,
            uptr               /*DataSize*/,
            const void*         /*Data*/
        );

        void* Reserved[2]; // Skip capsule and misc entries

        Status (*GetNextHighMonotonicCount)(
            u32*             /*HighCount*/
        );
        Status (*ResetSystem)(
            u32              /*ResetType*/,
            Status              /*ResetStatus*/,
            uptr               /*DataSize*/,
            void*               /*ResetData*/
        );
        Status (*UpdateCapsule)(
            void*               /*CapsuleHeaderArray*/,
            uptr               /*CapsuleCount*/,
            PhysicalAddr        /*ScatterGatherList*/
        );
        Status (*QueryCapsuleCapabilities)(
            void*               /*CapsuleHeaderArray*/,
            uptr               /*CapsuleCount*/,
            uptr*              /*MaximumCapsuleSize*/,
            u32*             /*ResetType*/
        );

        // Miscellaneous UEFI 2.0+ Services TODO
    };

    inline static SystemTable*     ST  = nullptr;
    inline static RuntimeServices* RS  = nullptr;

    static bool initializeSystemTable() {
        if (Limine::EFISupport::systemTableAvailable()) {
            ST = reinterpret_cast<SystemTable*>(
                     static_cast<uintptr_t>(Limine::EFISupport::efiSystemTableAddress()));
            RS = ST->RuntimeServices;
            return true;
        }
        return false;
    }

    struct [[gnu::packed]] MemoryDescriptor {
        u32       Type;
        u32       Pad;
        PhysicalAddr PhysicalStart;
        VirtualAddr  VirtualStart;
        u64       NumberOfPages;
        u64       Attribute;
        // (If DescriptorSize > sizeof(MemoryDescriptor), the extra bytes are undefined/padding.)
    };

    class MemoryMap {
    public:
        MemoryMap() {
            base        = reinterpret_cast<const u8*>(Limine::EFISupport::efiMemmapAddress());
            totalSize   = Limine::EFISupport::efiMemmapSize();
            descSize    = Limine::EFISupport::efiDescSize();
            descVersion = Limine::EFISupport::efiDescVersion();
            entryCount_ = (descSize == 0) ? 0 : (totalSize / descSize);
        }

        uptr entryCount() const noexcept {
            return entryCount_;
        }

        const MemoryDescriptor* descriptorAt(uptr idx) const noexcept {
            if (idx >= entryCount_) {
                return nullptr;
            }
            return reinterpret_cast<const MemoryDescriptor*>(
                base + (static_cast<usize>(idx) * static_cast<usize>(descSize))
            );
        }

        static constexpr bool isRuntime(const MemoryDescriptor* d) noexcept {
            return (d->Attribute & EFI_MEMORY_RUNTIME) != 0;
        }

        class RuntimeIterator {
        public:
            RuntimeIterator(const MemoryMap* parent_, uptr startIdx = 0) noexcept
              : parent(parent_), idx(startIdx)
            {
                advanceToNext();
            }

            bool valid() const noexcept {
                return (parent != nullptr) && (idx < parent->entryCount());
            }

            const MemoryDescriptor* operator*() const noexcept {
                return parent->descriptorAt(idx);
            }

            void next() noexcept {
                if (!valid()) {
                    return;
                }
                idx++;
                advanceToNext();
            }

        private:
            const MemoryMap* parent;
            uptr            idx;

            void advanceToNext() noexcept {
                while (idx < parent->entryCount()) {
                    const MemoryDescriptor* d = parent->descriptorAt(idx);
                    if (isRuntime(d)) {
                        break;
                    }
                    idx++;
                }
            }
        };

        RuntimeIterator runtimeBegin() const noexcept {
            return RuntimeIterator(this, 0);
        }
        RuntimeIterator runtimeEnd() const noexcept {
            return RuntimeIterator(this, entryCount());
        }

    private:
        const u8* base       = nullptr;
        u64         totalSize  = 0;
        u64         descSize   = 0;
        u64         descVersion= 0;
        uptr          entryCount_= 0;
    };

    //-------------------------------------------------------------------------
    //   - The caller must build an array of MemoryDescriptor entries where
    //     each runtime descriptor’s VirtualStart is set to the address at
    //     which it is mapped in the new page tables.
    //-------------------------------------------------------------------------
    static Status SetVirtualAddressMap(
        uptr   MemoryMapSize,
        uptr   DescriptorSize,
        uptr   DescriptorVersion,
        void*   VirtualMap
    ) noexcept {
        return RS->SetVirtualAddressMap(
            MemoryMapSize,
            DescriptorSize,
            DescriptorVersion,
            VirtualMap
        );
    }

    [[noreturn]] static void ResetSystem(
        ResetType  Type,
        Status     StatusCode,
        uptr      DataSize = 0,
        void*      ResetData = nullptr
    ) noexcept {
        RS->ResetSystem(
            static_cast<u32>(Type),
            StatusCode,
            DataSize,
            ResetData
        );
        __builtin_unreachable();
    }

    static Status GetTime(
        Time*              OutTime,
        TimeCapabilities*  OutCapabilities = nullptr
    ) noexcept {
        return RS->GetTime(OutTime, OutCapabilities);
    }

    static Status SetTime(
        const Time*        InTime
    ) noexcept {
        return RS->SetTime(InTime);
    }

    static Status GetVariable(
        Char16*            VariableName,
        const Guid*        VendorGuid,
        u32*            Attributes,
        uptr*             DataSize,
        void*              Data
    ) noexcept {
        return RS->GetVariable(
            VariableName,
            VendorGuid,
            Attributes,
            DataSize,
            Data
        );
    }

    static Status SetVariable(
        Char16*            VariableName,
        const Guid*        VendorGuid,
        u32             Attributes,
        uptr              DataSize,
        const void*        Data
    ) noexcept {
        return RS->SetVariable(
            VariableName,
            VendorGuid,
            Attributes,
            DataSize,
            Data
        );
    }

};

#endif

#endif //EFI_HH