OUTPUT_FORMAT(elf64-x86-64)
ENTRY(Supervisor_SystemStartup)

PHDRS
{
	text PT_LOAD FLAGS(5);
	rodata PT_LOAD FLAGS(4);
	data PT_LOAD FLAGS(6);
	dynamic PT_DYNAMIC FLAGS(6);
}

SECTIONS
{
	. = 0xffffffff80100000;

	__start = .;

	.text : {
		__text_start = .;
		*(.text .text.*)
		__text_end = .;
	} :text

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	__rodata_start = .;
	.rodata : {
		*(.rodata .rodata.*)
	} :rodata

	.limine_requests : {
		KEEP(*(.limine_requests_start))
		KEEP(*(.limine_requests))
		KEEP(*(.limine_requests_end))
	} :rodata
	__rodata_end = .;

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	.dynamic : {
		*(.dynamic)
	} :data :dynamic

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	.data : {
		__data_start = .;
		*(.data .data.*)
		__data_end = .;
	} :data

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	__percpu_start = .;
	.percpu : {
		*(.percpu .percpu.*)
	} :data
	__percpu_end = .;

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	.bss : {
		__bss_start = .;
		*(.bss .bss.*)
		*(COMMON)
		__bss_end = .;
	} :data

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	.symtab : {
		*(.symtab)
	}

	.strtab : {
		*(.strtab)
	}

	.shstrtab : {
		*(.shstrtab)
	}

	.debug_aranges : { *(.debug_aranges) }
	.debug_pubnames : { *(.debug_pubnames) }
	.debug_info : { *(.debug_info) }
	.debug_abbrev : { *(.debug_abbrev) }
	.debug_line : { *(.debug_line) }
	.debug_frame : { *(.debug_frame) }
	.debug_str : { *(.debug_str) }
	.debug_loc : { *(.debug_loc) }
	.debug_macinfo : { *(.debug_macinfo) }
	.debug_ranges : { *(.debug_ranges) }
	.debug_line_str : { *(.debug_line_str) }
	.debug_addr : { *(.debug_addr) }
	.debug_str_offsets : { *(.debug_str_offsets) }
	.debug_rnglists : { *(.debug_rnglists) }
	.debug_loclists : { *(.debug_loclists) }

	/DISCARD/ : {
		*(.eh_frame*)
		*(.note .note.*)
		*(.comment)
	}
}

