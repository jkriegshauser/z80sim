; TODO list
; 3. Convert IX/IY/SP to actual memory (not emulated offsets) (lose bounds checking??)
; 4. Prevent callbacks from being called in CPU thread context (i.e. messaging?)
; 5. Implement type 0 interrupts

        .386
        model  flat

        ; imported functions
        extrn   Sleep:near
        extrn   GetCurrentThreadId:near
        extrn   GetTickCount:near
        extrn   _memmove:near

domemwrite EQU 1

        ; emulated register equates
REGA    EQU     ah
REGF    EQU     al
REGAF   EQU     ax
REGB    EQU     ch
REGC    EQU     cl
REGBC   EQU     cx
REGDE   EQU     di
REGHL   EQU     si
REGSP   EQU     word ptr [_stack]
REGIX   EQU     word ptr [_ix]
REGIY   EQU     word ptr [_iy]
REGPC   EQU     ebp             ; the PC is stored as a pointer to actual memory, not an emulated offset
REGI    EQU     byte ptr [_ir+1]
REGR    EQU     byte ptr [_ir]
REGIR   EQU     word ptr [_ir]

; Interrupt jump point locations
NMI_LOC   EQU     066h
INT1_LOC  EQU     038h

; Times in milliseconds
ONESECOND EQU     1000

; Timing devisor (same used to calculate throttle interval)
DIVISOR   EQU     8
DIV2N     EQU     3     ; 2^n = DIVISOR where DIV2N = n
; Interval (in milliseconds) in which to check the cpu speed
INTERVAL  EQU     ONESECOND/DIVISOR

;******************************
;* _DATA segment              *
;******************************
; Static variables for tracking CPU status
_DATA   segment dword public use32 'DATA'
_running        db      0       ; boolean to track 'running' state
_stopped        db      1       ; boolean to track 'stopped' state (initially stopped)
_halted         db      0       ; CPU in special 'halt' state (waiting for interrupt)
_terminated     db      0       ; causes cpuloop() to exit on next instruction boundary if set
_memstart       dd      0       ; Parameter to cpuloop.  Must be a valid 64K memory size
_ir             dw      0h      ; interrupt/refresh register
_stack          dw      0ffffh  ; stack pointer
_ix             dw      0ffffh  ; x index register
_iy             dw      0ffffh  ; y index register
_iff            dw      0       ; interrupt flip flops
_inttype        db      0       ; interrupt mode (see im instruction)
_pending        dw      0       ; interrupt pending (first byte (+1)- bit 1=NMI, bit 0=intr, second byte (+0) - interrupt number)
_cycles         dd      0       ; clock cycle count
_cycleshi       dd      0       ; high dword of cycle count
_intdelay       db      0       ; iteration delay to enable interrupts
_dostep         db      0       ; instruction to stop after executing one instruction cycle
_undefproc      dd      0       ; function to call for undefined opcode
_clockproc      dd      0       ; function to call for clock ticks
_ioreadproc     dd      0       ; function to call for an I/O read
_iowriteproc    dd      0       ; function to call for an I/O write
_bpproc         dd      0       ; function to call for a breakpoint
_callbpproc     db      0       ; boolean to call breakpoint proc
_threadID       dd      -1      ; thread ID for the thread in the cpuloop() function
_cpqstarget     dd      0       ; target number of cycles per quarter second (250ms)
_cpqs           dd      0       ; number of cycles in last quarter second
_cpqslast       dd      0       ; previously recorded cycles per quarter second
_nextcheck      dd      0       ; time to do the next check of the timing system

; structure to keep track of all registers for saving and loading between running/stopped states
; registers followed by @ are alternate registers.
regs           struc
  af            dw      0ffffh
  af@           dw      0ffffh
  bc            dw      0ffffh
  bc@           dw      0ffffh
  de            dw      0ffffh
  de@           dw      0ffffh
  hl            dw      0ffffh
  hl@           dw      0ffffh
  ix            dw      0ffffh
  iy            dw      0ffffh
  stack         dw      0ffffh
  pc            dw      0h
  ir            dw      0ffffh
  iff           dw      0h
  inttype       db      0h
  intpending    dw      0h
  cycles        dd      0h
  cycleshi      dd      0h
  intdelay      db      0h
  halted        db      0h
regs           ends

; memory dedicated to saving register state
_z80regs        regs    ?

                align 4

; memory dedicated to breakpoints
_bparea         dd      30 dup(0)
_bparea_end:

; structure defining memory write storage
; startaddr and lastaddr define a range of memory.  If a byte is written in that
; range, the call to proc is made
memwrite      struc
  startaddr     dd      0h
  lastaddr      dd      0h
  proc          dd      0h
memwrite      ends

; memory dedicated to memory write callbacks
_memwritearea   memwrite  30 dup(?)
_memwritearea_end:

_DATA   ends

_TEXT	segment dword public use32 'CODE'

; starts the cpu
; returns immediately (does not wait for CPU thread to acknowledge start)
; has no effect if CPU is already running
; C-callable as: void start(void)
        align 4
        public _z80_start
_z80_start          proc    near
        mov     byte ptr [_running],1
        ret
_z80_start          endp

; returns the status of the CPU
; C-callable as: bool is_running(void)
        align 4
        public _z80_is_running
_z80_is_running     proc    near
        xor     eax,eax
        or      byte ptr [_stopped],0
        jnz     is_running_ret
        inc     eax
is_running_ret:
        ret
_z80_is_running     endp

; stops the cpu (if not already stopped) and blocks waiting for current instruction to finish
; returns only when CPU has acknowledged stop and saved register state for host retrieval
; NOTE: returns immediately if called from CPU thread context (i.e. in callback)
; C-callable as: void stop(void)
        align 4
        public _z80_stop
_z80_stop           proc    near
        or      byte ptr [_stopped],0
        jnz     _stop_ret                 ; return if already stopped
        pushad                            ; save registers
        call    GetCurrentThreadId        ; get our context ID
        cmp     eax,dword ptr [_threadID]
        popad                             ; restore registers
        mov     byte ptr [_running],0     ; tell the CPU thread to stop running
        je      _stop_ret                 ; we're in the cpu thread, so don't loop or we'll lock up
_stop_loop:
        pushd   1
        call    Sleep                     ; do context switch if not stopped yet
        or      byte ptr [_stopped],0
        jz      _stop_loop                ; break out if CPU has stopped
_stop_ret:
        ret
_z80_stop           endp

; Returns the CPUs current state (boolean)
; C-callable as: int is_stopped(void)
        align 4
        public _z80_is_stopped
_z80_is_stopped      proc    near
        movzx   eax,byte ptr [_stopped]
        ret
_z80_is_stopped      endp

; instructs the cpu thread to exit the cpuloop() function after the current opcode completes
; returns immediately
; void terminate(void);
        align 4
        public _z80_terminate
_z80_terminate      proc    near
        mov     byte ptr [_terminated],1
        ret
_z80_terminate      endp

; return the current number of cpu cycles
; If the 'high' pointer is non-NULL, the high 4-bytes of the cycle count is returned there
; C-callable as: unsigned long cycles(unsigned long *high);
        align 4
        public _z80_get_cycles
_z80_get_cycles     proc    near
        cmp     dword ptr [esp+4],0
        je      _get_cycles_donehigh
        mov     ebx,dword ptr [esp+4]
        mov     ecx,dword ptr [_z80regs.cycleshi]
        mov     dword ptr [ebx],ecx
_get_cycles_donehigh:
        mov     eax,dword ptr [_z80regs.cycles]
        ret
_z80_get_cycles     endp

; Sets the target emulation speed in Hz (I.E. 4 MHz would be 4000000)
; C-callable as: void set_target_cps(unsigned long cps)
        align 4
        public  _z80_set_target_cps
_z80_set_target_cps proc    near
        mov     eax,dword ptr [esp+4]
        shr     eax,DIV2N
        mov     dword ptr [_cpqstarget],eax
        ret
_z80_set_target_cps endp

; Returns an estimated number of clock cycles recorded over the last second
; C-callable as: unsigned long get_last_cps()
        align 4
        public  _z80_get_last_cps
_z80_get_last_cps   proc    near
        mov     eax,dword ptr [_cpqslast]
        shl     eax,DIV2N
        ret
_z80_get_last_cps   endp

; resets the CPU registers, interrupt flip-flops, interrupt mode and cycle count
; to power-on defaults.  Stops CPU if necessary before resetting registers
; C-callable as: void reset(void)
        align 4
        public _z80_reset
_z80_reset          proc    near
        call    _z80_stop
        mov     ebx,-1
        mov     dword ptr [_z80regs.af],ebx
        mov     dword ptr [_z80regs.bc],ebx
        mov     dword ptr [_z80regs.de],ebx
        mov     dword ptr [_z80regs.hl],ebx
        mov     word ptr [_z80regs.ix],bx
        mov     word ptr [_z80regs.iy],bx
        mov     word ptr [_z80regs.stack],bx
        mov     word ptr [_z80regs.ir],bx
        xor     ebx,ebx
        mov     word ptr [_z80regs.intpending],bx
        mov     word ptr [_z80regs.pc],bx
        mov     word ptr [_z80regs.iff],bx
        mov     byte ptr [_z80regs.inttype],bl
        mov     dword ptr [_z80regs.cycles],ebx
        mov     dword ptr [_z80regs.cycleshi],ebx
        mov     byte ptr [_z80regs.intdelay],bl
        mov     byte ptr [_z80regs.halted],bl
        ret
_z80_reset          endp

; Saves the CPU register context.  Does not modify any cpu registers.  Destroys
; ebx.
        align 4
loadregs       proc    near
        mov     eax,dword ptr [_z80regs.af]
        mov     ecx,dword ptr [_z80regs.bc]
        mov     edi,dword ptr [_z80regs.de]
        mov     esi,dword ptr [_z80regs.hl]
        mov     bx,word ptr [_z80regs.ix]
        mov     REGIX,bx
        mov     bx,word ptr [_z80regs.iy]
        mov     REGIY,bx
        mov     bx,word ptr [_z80regs.stack]
        mov     REGSP,bx
        mov     bx,word ptr [_z80regs.ir]
        mov     REGIR,bx
        movzx   REGPC,word ptr [_z80regs.pc]
        add     REGPC,dword ptr [_memstart]
        mov     bx,word ptr [_z80regs.iff]
        mov     word ptr [_iff],bx
        mov     bl,byte ptr [_z80regs.inttype]
        mov     byte ptr [_inttype],bl
        mov     bx,word ptr [_z80regs.intpending]
        mov     word ptr [_pending],bx
        mov     ebx,dword ptr [_z80regs.cycles]
        mov     dword ptr [_cycles],ebx
        mov     ebx,dword ptr [_z80regs.cycleshi]
        mov     dword ptr [_cycleshi],ebx
        mov     bl,byte ptr [_z80regs.intdelay]
        mov     byte ptr [_intdelay],bl
        mov     bl,byte ptr [_z80regs.halted]
        mov     byte ptr [_halted],bl
        ret
loadregs       endp

; Loads the saved register context.  Does not modify saved context.  Destroys ebx
        align 4
saveregs       proc    near
        mov     dword ptr [_z80regs.af],eax
        mov     dword ptr [_z80regs.bc],ecx
        mov     dword ptr [_z80regs.de],edi
        mov     dword ptr [_z80regs.hl],esi
        mov     bx,REGIX
        mov     word ptr [_z80regs.ix],bx
        mov     bx,REGIY
        mov     word ptr [_z80regs.iy],bx
        mov     bx,REGSP
        mov     word ptr [_z80regs.stack],bx
        mov     bx,REGIR
        mov     word ptr [_z80regs.ir],bx
        mov     ebx,REGPC
        sub     ebx,dword ptr [_memstart]
        mov     word ptr [_z80regs.pc],bx
        mov     bx,word ptr [_iff]
        mov     word ptr [_z80regs.iff],bx
        mov     bl,byte ptr [_inttype]
        mov     byte ptr [_z80regs.inttype],bl
        mov     bx,word ptr [_pending]
        mov     word ptr [_z80regs.intpending],bx
        mov     ebx,dword ptr [_cycles]
        mov     dword ptr [_z80regs.cycles],ebx
        mov     ebx,dword ptr [_cycleshi]
        mov     dword ptr [_z80regs.cycleshi],ebx
        mov     bl,byte ptr [_intdelay]
        mov     byte ptr [_z80regs.intdelay],bl
        mov     bl,byte ptr [_halted]
        mov     byte ptr [_z80regs.halted],bl
        ret
saveregs       endp

; Main loop for CPU execution.  DOES NOT RETURN until terminate() is called.
; Use the start() and stop() functions to control execution.  Register states can
; be modified while CPU is stopped via the pointer from getregs().
; MUST pass valid non-NULL pointer to 64K (65536 byte) memory location.
; C-callable as: void cpuloop(unsigned char *mem)
        align 4
        public _z80_cpuloop
_z80_cpuloop	proc	near
        mov     ebx,dword ptr [esp+4]           ; get memstart address
        mov     dword ptr [_memstart],ebx
        pushad                                  ; save called context for returning later
        call    GetCurrentThreadId              ; save the thread ID
        mov     dword ptr [_threadID],eax
        call    _z80_reset                      ; reset registers to power-on defaults
cpuloop_loop:
        cmp     dword ptr [_opcodetable],offset doNOP
        je      debug_skip
        mov     dword ptr [_opcodetable],offset doNOP
debug_skip:
        or      byte ptr [_running],0
        jnz     short cpuloop_running           ; jump if running
        or      byte ptr [_stopped],0
        pushad                                  ; save regs
        jnz     short cpuloop_sleep             ; jump if not already stopped
        ; At this point, the CPU is not running, but not 'stopped' either--
        ; -- meaning that the register context hasn't been saved
cpuloop_savecontext:
        call    saveregs                        ; save the registers when first stopped
        mov     byte ptr [_stopped],1
        or      byte ptr [_callbpproc],0
        ; pushad                                  ; save regs
        jz      short cpuloop_sleep             ; jump if we don't need to call the breakpoint proc
        mov     byte ptr [_callbpproc],0        ; reset the flag to call breakpoint proc
        or      dword ptr [_bpproc],0
        jz      short cpuloop_sleep             ; jump if no breakpoint proc
        mov     ebx,REGPC
        sub     ebx,dword ptr [_memstart]
        push    ebx                             ; push current PC
        call    dword ptr [_bpproc]             ; call breakpoint proc
        pop     ebx
cpuloop_sleep:
        pushd   1
        call    Sleep                           ; Context switch
        popad                                   ; restore regs
        jmp     cpuloop_checkterminated         ; jump to loop bottom
cpuloop_running:
        or      byte ptr [_stopped],0
        jz      short cpuloop_run               ; jump if not stopped
        ; CPU was stopped, so reload registers
        mov     byte ptr [_stopped],0
        call    loadregs
cpuloop_run:
        pushad
        call    GetTickCount
        cmp     eax,dword ptr [_nextcheck]
        jb      cpuloop_aftercheck
cpuloop_resetclock:
        add     eax,INTERVAL
        mov     dword ptr [_nextcheck],eax
        mov     eax,dword ptr [_cpqs]
        mov     dword ptr [_cpqslast],eax
        mov     dword ptr [_cpqs],0
cpuloop_aftercheck:
        popad
        ; throttle the CPU as necessary
        mov     ebx,dword ptr [_cpqstarget]
        or      ebx,ebx
        jz      cpuloop_checkintr
        cmp     dword ptr [_cpqs],ebx
        jae     cpuloop_contextswitch
cpuloop_checkintr:
        ; check for interrupt and handle it
        call    check_interrupt
        ; clear ebx
        xor     ebx,ebx                         ; effectively NOP opcode
        ; check if CPU is halted
        or      byte ptr [_halted],0
        jz      cpuloop_nothalted               ; not halted, continue with next instruction
        dec     REGPC                           ; decrement increment from NOP instruction
        jmp     cpuloop_do_op                   ; halted so continue with NOP
cpuloop_nothalted:
        mov     bl,byte ptr [REGPC]
cpuloop_do_op:
        ; DEBUG code
        cmp     ebx,255
        jle     cpuloop_do_op_debug
        jmp     $
cpuloop_do_op_debug:
        push    ebx                             ; push opcode number
        call    dword ptr [_opcodetable + ebx*4]  ; call opcode function
        or      dword ptr [_clockproc],0
        jz      cpuloop_skipclock
        pushad
        push    ebx
        call    dword ptr [_clockproc]
        pop     ebx
        popad
cpuloop_skipclock:
        add     dword ptr [_cpqs],ebx           ; add to number of cycles per quarter second
        add     dword ptr [_cycles],ebx         ; number of cycles returned in ebx
        adc     dword ptr [_cycleshi],0         ; do jnc/inc instead?
        add     esp,4
        ; increment R register
        mov     bl,REGR
        and     REGR,080h                       ; save high bit (user defined)
        inc     bl
        and     bl,07fh
        or      REGR,bl
        ; need to enable interrupts?
        cmp     byte ptr [_intdelay],0
        jle     cpuloop_intrdone
        dec     byte ptr [_intdelay]
        jnz     cpuloop_intrdone
        mov     word ptr [_iff],0101h           ; turn on iff1 and iff2
cpuloop_intrdone:
        ; check for single-step
        or      byte ptr [_dostep],0
        jz      short cpuloop_checkhalt         ; jump if not single-stepping
        mov     byte ptr [_running],0           ; stop the CPU because of single-stepping
        mov     byte ptr [_dostep],0
        jmp     short cpuloop_checkbp           ; skip halt check
cpuloop_checkhalt:
        ; check if CPU is halted and perform context switch if halted
        or      byte ptr [_halted],0
        jz      cpuloop_checkbp                 ; not halted, don't perform context switch
cpuloop_contextswitch:
        pushad                                  ; save regs
        pushd   1
        call    Sleep                           ; do context switch
        popad
cpuloop_checkbp:
        ; check for breakpoint.  Sets a flag if break is needed.  Break happens at top of loop
        call    check_breakpoint
cpuloop_checkterminated:
        or      byte ptr [_terminated],0
        jz      cpuloop_loop
        ; if we make it here, we're exiting the cpu function
        popad
        ret
_z80_cpuloop        endp

; Called to single-step the CPU.  Executes one instruction and stops the CPU.  If
; an interrupt is pending, execute the first instruction in the ISR
; waits (blocks) for the instruction to complete
; C-callable as: void step(void)
        public _z80_step
_z80_step           proc    near
        mov     byte ptr [_dostep],1            ; set dostep flag
        call    _z80_start                      ; start the CPU
        jmp     step_check
step_loop:
        pushd   1
        call    Sleep                           ; context switch
step_check:
        cmp     byte ptr [_dostep],0
        jne     step_loop
step_ret:
        ret
_z80_step           endp

; Returns whether or not the CPU is being single-stepped.  This is useful only by
; callback functions that are called during the instruction cycle (i.e. memwrite events)
; C-callable as: int z80_is_stepped(void)
        public  _z80_is_stepped
_z80_is_stepped     proc    near
        movzx   eax,byte ptr [_dostep]
        ret
_z80_is_stepped     endp

; Interrupt the CPU to be handled on the next instruction cycle
; return is boolean
; C-callable as: int interrupt(char num)
        public _z80_interrupt
_z80_interrupt      proc    near
        push    ebx
        xor     eax,eax
        cmp     byte ptr [_iff],0               ; can't interrupt if flip-flops are disabled
        je      _interrupt_ret
        inc     eax
        mov     ebx,dword ptr [esp+8]
        mov     bh,byte ptr [_pending+1]
        or      bh,01h                          ; turn on interrupt pending bit
        mov     word ptr [_pending],bx          ; save pending information to both current and stored locs
        mov     word ptr [_z80regs.intpending],bx
_interrupt_ret:
        pop     ebx
        ret
_z80_interrupt      endp

; Interrupt the CPU with an NMI to be handled on the next instruction cycle
; C-callable as: void nmi(void)
                align 4
                public _z80_nmi
_z80_nmi            proc    near
        or      byte ptr [_pending+1],02h       ; turn on NMI pending bit
        or      byte ptr [_z80regs.intpending+1],02h
        ret
_z80_nmi            endp

; Returns the pointer to the stored register state.  This pointer can be used to
; retrieve or modify register states.  Modifications made while the CPU is running
; will be discarded.  The states retrieved are from the last time the CPU was stopped
; C-callable as: Z80REGS *getregs(void)
                align 4
                public _z80_getregs
_z80_getregs        proc    near
        mov     eax,offset _z80regs
        ret
_z80_getregs        endp

; Returns a pointer to the memory area that was given to the processor thread at
; entry into the z80_cpuloop() function.
; C-callable as: unsigned char *z80_get_mem()
                align 4
                public  _z80_get_mem
_z80_get_mem    proc    near
        mov     eax,dword ptr [_memstart]
        ret
_z80_get_mem    endp

; Checks for any pending interrupts.  NMIs have highest priority and are checked
; first.
                align 4
check_interrupt proc    near
        ; check for NMI first
        test    byte ptr [_pending+1],02h
        jz      check_interrupt_after_nmi       ; jump if no NMI is pending
        mov     ebx,5
        cmp     dword ptr [_clockproc],0
        je      checkintr_skipclock
        pushad
        push    ebx
        call    dword ptr [_clockproc]
        pop     ebx
        popad
checkintr_skipclock:
        add     dword ptr [_cycles],ebx
        adc     dword ptr [_cycleshi],0         ; do jnc/inc instead?
        mov     edx,REGPC
        sub     edx,dword ptr [_memstart]
        call    push_dx                         ; save return address on virtual stack
        xor     REGPC,REGPC
        mov     bp,NMI_LOC                      ; load PC with NMI restart location
        add     REGPC,dword ptr [_memstart]
        mov     byte ptr [_iff],0               ; clear iff1
        and     byte ptr [_pending+1],0fdh      ; clear NMI pending bit
        mov     byte ptr [_halted],0            ; CPU is no longer halted
        jmp     check_interrupt_ret             ; return
check_interrupt_after_nmi:
        test    byte ptr [_pending+1],01h
        jz      check_interrupt_ret             ; jump if no interrupt is pending
        cmp     byte ptr [_iff],0
        je      check_interrupt_done            ; can't accept interrupt if disabled
        mov     word ptr [_iff],0               ; clear iff1 and iff2
        mov     byte ptr [_halted],0            ; CPU is no longer halted
        cmp     byte ptr [_inttype],1           ; check interrupt type
        jb      check_interrupt_type0
        je      check_interrupt_type1
        ; default type - type 2 (vectored interrupts)
        mov     ebx,19
        cmp     dword ptr [_clockproc],0
        je      checkintr_skipclock2
        pushad
        push    ebx
        call    dword ptr [_clockproc]
        pop     ebx
        popad
checkintr_skipclock2:
        add     dword ptr [_cycles],ebx
        adc     dword ptr [_cycleshi],0         ; do jnc/inc instead?
        mov     edx,REGPC
        sub     edx,dword ptr [_memstart]
        call    push_dx                         ; save return location on virtual stack
        movzx   ebx,REGIR                       ; calculate vector address
        mov     bl,byte ptr [_pending]
        and     bl,0feh
        add     ebx,dword ptr [_memstart]
        movzx   REGPC,word ptr [ebx]            ; get interrupt location from vector table
        add     REGPC,dword ptr [_memstart]
        jmp     short check_interrupt_done
check_interrupt_type1:
        mov     ebx,5
        cmp     dword ptr [_clockproc],0
        je      checkintr_skipclock1
        pushad
        push    ebx
        call    dword ptr [_clockproc]
        pop     ebx
        popad
checkintr_skipclock1:
        add     dword ptr [_cycles],ebx
        adc     dword ptr [_cycleshi],0         ; do jnc/inc instead?
        mov     edx,REGPC
        sub     edx,dword ptr [_memstart]
        call    push_dx
        xor     REGPC,REGPC
        mov     bp,INT1_LOC                     ; load PC with INT1 restart location
        add     REGPC,dword ptr [_memstart]
        jmp     short check_interrupt_done
check_interrupt_type0:
check_interrupt_done:
        and     byte ptr [_pending+1],0feh      ; clear interrupt pending bit
check_interrupt_ret:
        ret
check_interrupt endp

; pushes a value on to the emulated stack and decrements emulated stack pointer
; value to push in dx.  destroys ebx
                align 4
push_dx         proc    near
        sub     REGSP,2
        movzx   ebx,REGSP
        add     ebx,dword ptr [_memstart]
        mov     word ptr [ebx],dx
        ifdef   domemwrite
        call    check_memwrite
        endif
        ret
push_dx         endp

; destroys ebx - return val in bx
                align 4
pop_bx          proc    near
        movzx   ebx,REGSP
        add     ebx,dword ptr [_memstart]
        movzx   ebx,word ptr [ebx]
pop_done:
        add     REGSP,2
        ret
pop_bx          endp

; destroys edx - return val in dx
                align 4
pop_dx          proc    near
        movzx   edx,REGSP
        add     edx,dword ptr [_memstart]
        movzx   edx,word ptr [edx]
        jmp     short pop_done
pop_dx          endp

        align 4
; UNDEFPROC set_undef_proc(UNDEFPROC newundefproc);
        public  _z80_set_undef_proc
_z80_set_undef_proc proc    near
        push    edx
        mov     eax,dword ptr [_undefproc]
        mov     edx,dword ptr [esp+8]
        mov     dword ptr [_undefproc],edx
        pop     edx
        ret
_z80_set_undef_proc endp

        align 4
        public  _z80_set_clock_proc
_z80_set_clock_proc   proc  near
        push    edx
        mov     eax,dword ptr [_clockproc]
        mov     edx,dword ptr [esp+8]
        mov     dword ptr [_clockproc],edx
        pop     edx
        ret
_z80_set_clock_proc   endp

        align 4
        public  _z80_set_io_read_proc
_z80_set_io_read_proc proc  near
        push    edx
        mov     eax,dword ptr [_ioreadproc]
        mov     edx,dword ptr [esp+8]
        mov     dword ptr [_ioreadproc],edx
        pop     edx
        ret
_z80_set_io_read_proc endp

        align 4
        public  _z80_set_io_write_proc
_z80_set_io_write_proc proc  near
        push    edx
        mov     eax,dword ptr [_iowriteproc]
        mov     edx,dword ptr [esp+8]
        mov     dword ptr [_iowriteproc],edx
        pop     edx
        ret
_z80_set_io_write_proc endp

        align 4
        public  _z80_set_breakpoint_proc
_z80_set_breakpoint_proc proc  near
        push    edx
        mov     eax,dword ptr [_bpproc]
        mov     edx,dword ptr [esp+8]
        mov     dword ptr [_bpproc],edx
        pop     edx
        ret
_z80_set_breakpoint_proc endp

; regsters the breakpoint passed on the stack
        align 4
        public _z80_register_breakpoint
_z80_register_breakpoint  proc  near
        push    ebp
        mov     ebp,esp
        push    ebx
        push    edx
        xor     eax,eax
        mov     ebx,offset _bparea
rb_loop:
        cmp     ebx,offset _bparea_end
        jae     short rb_ret
        or      dword ptr [ebx],0
        jz      short rb_found
        add     ebx,4
        jmp     short rb_loop
rb_found:
        inc     eax
        movzx   edx,word ptr [ebp+8]
        add     edx,dword ptr [_memstart]
        mov     dword ptr [ebx],edx
rb_ret:
        pop     edx
        pop     ebx
        pop     ebp
        ret
_z80_register_breakpoint  endp

; unregisters the breakpoint passed on the stack
        align 4
        public _z80_unregister_breakpoint
_z80_unregister_breakpoint  proc  near
        push    ebp
        mov     ebp,esp
        push    ebx
        push    edx
        xor     eax,eax
        mov     ebx,offset _bparea
        movzx   edx,word ptr [ebp+8]
        add     edx,dword ptr [_memstart]
ub_loop:
        cmp     ebx,offset _bparea_end
        jae     short ub_ret
        or      dword ptr [ebx],0
        jz      short ub_ret
        cmp     dword ptr [ebx],edx
        je      short ub_found
        add     ebx,4
        jmp     short ub_loop
ub_found:
        push    ecx
        push    esi
        push    edi
        inc     eax
        cld
        mov     edi,ebx
        add     ebx,4
        mov     esi,ebx
        mov     ecx,offset _bparea_end
        sub     ecx,ebx
        shr     ecx,2
        rep     movsd
        mov     dword ptr [edi],0
        pop     edi
        pop     esi
        pop     ecx
ub_ret:
        pop     edx
        pop     ebx
        pop     ebp
        ret
_z80_unregister_breakpoint  endp

; checks for a breakpoint at the current PC location.  sets the _callbpproc flag if necessary
        align 4
check_breakpoint  proc  near
        mov     ebx,offset _bparea
cb_loop:
        cmp     ebx,offset _bparea_end
        jae     cb_ret
        or      dword ptr [ebx],0
        jz      cb_ret
        cmp     dword ptr [ebx],REGPC
        je      cb_found
        add     ebx,4
        jmp     cb_loop
cb_found:
        or      byte ptr [_running],0
        je      cb_ret
        mov     byte ptr [_running],0
        mov     byte ptr [_callbpproc],1
cb_ret:
        ret
check_breakpoint  endp

; regsters the memory write callback passed on the stack
        align 4
        public _z80_register_memwrite
_z80_register_memwrite    proc  near
        push    ebp
        mov     ebp,esp
        push    ebx
        push    edx
        xor     eax,eax
        mov     ebx,offset _memwritearea
rm_loop:
        cmp     ebx,offset _memwritearea_end
        jae     short rm_ret
        or      dword ptr [ebx.startaddr],0
        jz      short rm_found
        add     ebx,size memwrite
        jmp     short rm_loop
rm_found:
        movzx   edx,word ptr [ebp+8]
        mov     eax,edx
        shl     eax,16
        add     edx,dword ptr [_memstart]
        mov     dword ptr [ebx.startaddr],edx
        movzx   edx,word ptr [ebp+12]
        mov     ax,dx
        add     edx,dword ptr [_memstart]
        mov     dword ptr [ebx.lastaddr],edx
        mov     edx,dword ptr [ebp+16]
        mov     dword ptr [ebx.proc],edx
        xor     eax,edx
rm_ret:
        pop     edx
        pop     ebx
        pop     ebp
        ret
_z80_register_memwrite    endp

; unregisters the memory write callback passed on the stack
        align 4
        public _z80_unregister_memwrite
_z80_unregister_memwrite    proc  near
        push    ebp
        mov     ebp,esp
        push    ebx
        push    edx
        xor     eax,eax
        mov     ebx,offset _memwritearea
um_loop:
        cmp     ebx,offset _memwritearea_end
        jae     short um_ret
        or      dword ptr [ebx],0
        jz      short um_ret
        mov     edx,dword ptr [ebx.startaddr]
        sub     edx,dword ptr [_memstart]
        shl     edx,16
        mov     ecx,dword ptr [ebx.lastaddr]
        sub     ecx,dword ptr [_memstart]
        mov     dx,cx
        xor     edx,dword ptr [ebx.proc]
        cmp     dword ptr [ebp+8],edx
        je      short um_found
        add     ebx,size memwrite
        jmp     short um_loop
um_found:
        push    ecx
        push    esi
        push    edi
        inc     eax
        cld
        mov     edi,ebx
        add     ebx,size memwrite
        mov     esi,ebx
        mov     ecx,offset _memwritearea_end
        sub     ecx,ebx
        shr     ecx,2
        rep     movsd
        mov     dword ptr [edi],0
        pop     edi
        pop     esi
        pop     ecx
um_ret:
        pop     edx
        pop     ebx
        pop     ebp
        ret
_z80_unregister_memwrite    endp

; calls the correct mem write handler for the physical address in ebx
; destroys edx (preserves all others)
        align 4
check_memwrite    proc  near
        pushfd
        mov     edx,offset _memwritearea
cm_loop:
        cmp     edx,offset _memwritearea_end
        jae     cm_ret
        or      dword ptr [edx.startaddr],0
        jz      cm_ret
        cmp     ebx,dword ptr [edx.startaddr]
        jb      cm_next
        cmp     ebx,dword ptr [edx.lastaddr]
        ja      cm_next
        ; found a callback to call
        pushad
        sub     ebx,dword ptr [_memstart]
        push    ebx
        call    dword ptr [edx.proc]
        pop     ebx
        popad
cm_next:
        add     edx,size memwrite
        jmp     cm_loop
cm_ret:
        popfd
        ret
check_memwrite    endp

; undefined opcode handler.
; calls the undefined opcode callback if set and stops the CPU
        align 4
undefined       proc    near
        mov     byte ptr [_running],0       ; stop the CPU in the event of an undefined opcode
        cmp     dword ptr [_undefproc],0
        je      undefined_ret
        mov     ebx,dword ptr [esp+4]
        pushad
        push    ebx                         ; push opcode number
        call    dword ptr [_undefproc]
        add     esp,4
        popad
undefined_ret:
        xor     ebx,ebx
        ret
undefined       endp

        align 4
setflagsinc8:
        pushfd
        and     REGF,01h    ; mask carry flag
        and     bl,028h    ; mask x and y flags (supposedly unused)
        or      REGF,bl
        pop     ebx
        and     bl,0d0h    ; mask off flags to keep
        or      REGF,bl
        and     bh,08h  ; test overflow flag
        shr     bh,1    ; move into correct location
        or      REGF,bh
        ret

; opcode functions must ALWAYS return the number of clock cycles they used in ebx and should increment the pc (REGPC) the number of bytes used (including arguments)
; opcode functions are provided with their number as the first parameter on the stack (esp+8)
; opcode functions must preserve ALL GP registers except for ebx and edx

; NO oPeration
; wastes clock cycles by doing nothing
        align 4
doNOP:
        inc     REGPC
        mov     ebx,4
        ret

; Handles the DD prefix by calling the function from the DD codepage
; R register gets incremented here and in the main loop
        align 4
doDD    proc    near
        inc     REGPC
        ; call function for next opcode
        xor     ebx,ebx
        mov     bl,byte ptr [REGPC]
        push    ebx                                 ; push function number
        call    dword ptr [_opcodetableDD + ebx*4]  ; call function
        add     esp,4
        ; increment R register
        mov     dl,REGR
        and     REGR,080h
        inc     dl
        and     dl,07fh
        or      REGR,dl
        ret
doDD    endp

; Handles the FD prefix by calling the function from the FD codepage
; R register gets incremented here and in the main loop
        align 4
doFD    proc    near
        inc     REGPC
        ; call function for next opcode
        xor     ebx,ebx
        mov     bl,byte ptr [REGPC]
        push    ebx                                 ; push function number
        call    dword ptr [_opcodetableFD + ebx*4]  ; call function
        add     esp,4
        ; increment R register
        mov     dl,REGR
        and     REGR,080h
        inc     dl
        and     dl,07fh
        or      REGR,dl
        ret
doFD    endp

; Handles the ED prefix by calling the function from the ED codepage
; R register gets incremented here and in the main loop
        align 4
doED    proc    near
        inc     REGPC
        ; call function for next opcode
        xor     ebx,ebx
        mov     bl,byte ptr [REGPC]
        push    ebx                                 ; push function number
        call    dword ptr [_opcodetableED + ebx*4]  ; call function
        add     esp,4
        ; increment R register
        mov     dl,REGR
        and     REGR,080h
        inc     dl
        and     dl,07fh
        or      REGR,dl
        ret
doED    endp

; Handles the CB prefix by calling the function from the CB codepage, assuming
; that indirect operations use the HL register
; R register gets incremented here and in the main loop
        align 4
doCB    proc    near
        inc     REGPC
        ; call function for next opcode
        xor     edx,edx
        mov     dl,byte ptr [REGPC]
        push    edx                                 ; push function number
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]           ; important!!!  ebx always points to (HL) for operations that use HL in an indirect manner
        call    dword ptr [_opcodetableCB + edx*4]  ; call function
        add     esp,4
        ; increment R register
        mov     dl,REGR
        and     REGR,080h
        inc     dl
        and     dl,07fh
        or      REGR,dl
        ret
doCB    endp

; Handles the DDCB prefix by calling the function from the CB codepage, assuming
; that indirect operations use the IX index register
; R register gets incremented here and in the main loop
        align 4
doDDCB  proc    near
        ; call function for next opcode
        movzx   edx,byte ptr [REGPC+2]
        push    edx                                 ; push function number
        call    deref_ix_ebx                        ; dereference (ix+n) for default indirect operations
        inc     REGPC                               ; get one increment from derefrencing the pointer
        mov     edx,dword ptr [esp]
        call    dword ptr [_opcodetableCB + edx*4]  ; call function
        add     esp,4
        add     ebx,8
        ret
doDDCB  endp

; Handles the FDCB prefix by calling the function from the CB codepage, assuming
; that indirect operations use the IY index register
; R register gets incremented here and in the main loop
        align 4
doFDCB  proc    near
        ; call function for next opcode
        xor     edx,edx
        mov     dl,byte ptr [REGPC+2]
        push    edx                                 ; push function number
        call    deref_iy_ebx                        ; dereference (iy+n) for default indirect operations
        inc     REGPC                               ; get one increment from derefrencing the pointer
        mov     edx,dword ptr [esp]
        call    dword ptr [_opcodetableCB + edx*4]  ; call function
        add     esp,4
        add     ebx,8
        ret
doFDCB  endp

        align 4
ld_bcnn:
        mov     REGBC,word ptr [REGPC+1]
        jmp     ld_ssnn_done
ld_denn:
        mov     REGDE,word ptr [REGPC+1]
        jmp     ld_ssnn_done
ld_hlnn:
        mov     REGHL,word ptr [REGPC+1]
        jmp     ld_ssnn_done
ld_spnn:
        mov     bx,word ptr [REGPC+1]
        mov     REGSP,bx
ld_ssnn_done:
        mov     ebx,10
ld_ssnn_ret:
        add     REGPC,3
        ret
ld_ix_nn:
        mov     bx,word ptr [REGPC+1]
        mov     REGIX,bx
        jmp     ld_index_n_done
ld_iy_nn:
        mov     bx,word ptr [REGPC+1]
        mov     REGIY,bx
ld_index_n_done:
        add     REGPC,3
        mov     ebx,14
        ret

        align 4
ld_dei_a:
        movzx   ebx,REGDE
        add     ebx,dword ptr [_memstart]
        mov     byte ptr [ebx],REGA
        jmp     ld_aind_done
ld_bcia:
        movzx   ebx,REGBC
        add     ebx,dword ptr [_memstart]
        mov     byte ptr [ebx],REGA
ld_aind_done:
        ifdef   domemwrite
        call    check_memwrite
        endif
        inc     REGPC
        mov     ebx,7
        ret
ld_ext_a:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     byte ptr [ebx],REGA
        ifdef   domemwrite
        call    check_memwrite
        endif
        add     REGPC,3
        mov     ebx,13
        ret

        align 4
inc_bc:
        inc     REGBC
        jmp     inc_ss_done
inc_de:
        inc     REGDE
        jmp     inc_ss_done
inc_hl:
        inc     REGHL
        jmp     inc_ss_done
inc_sp:
        inc     REGSP
inc_ss_done:
        mov     ebx,6
        inc     REGPC
        ret
inc_ix:
        inc     REGIX
        jmp     inc_ixy_done
inc_iy:
        inc     REGIY
inc_ixy_done:
        mov     ebx,10
        inc     REGPC
        ret

        align 4
dec_bc:
        dec     REGBC
        jmp     dec_ss_done
dec_de:
        dec     REGDE
        jmp     dec_ss_done
dec_hl:
        dec     REGHL
        jmp     dec_ss_done
dec_sp:
        dec     REGSP
dec_ss_done:
        mov     ebx,6
        inc     REGPC
        ret
dec_ix:
        dec     REGIX
        jmp     dec_ixy_done
dec_iy:
        dec     REGIY
dec_ixy_done:
        mov     ebx,10
        inc     REGPC
        ret

        align 4
inc_b:
        inc     REGB
        mov     bl,REGB
        jmp     inc_r_done
inc_c:
        inc     REGC
        mov     bl,REGC
        jmp     inc_r_done
inc_d:
        mov     bx,REGDE
        inc     bh
        mov     REGDE,bx
        mov     bl,bh
        jmp     inc_r_done
inc_e:
        mov     bx,REGDE
        inc     bl
        mov     REGDE,bx
        jmp     inc_r_done
inc_h:
        mov     bx,REGHL
        inc     bh
        mov     REGHL,bx
        mov     bl,bh
        jmp     inc_r_done
inc_l:
        mov     bx,REGHL
        inc     bl
        mov     REGHL,bx
        jmp     inc_r_done
inc_a:
        inc     REGA
        mov     bl,REGA
inc_r_done:
        call    setflagsinc8
        inc     REGPC
        mov     ebx,4
        ret

        align 4
inc_hli:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        inc     byte ptr [ebx]
        ifdef   domemwrite
        call    check_memwrite
        endif
        mov     ebx,11
        jmp     inc_indirect
inc_ixi:
        call    deref_ix_ebx
        jmp     do_inc
inc_iyi:
        call    deref_iy_ebx
do_inc:
        inc     byte ptr [ebx]
        ifdef   domemwrite
        call    check_memwrite
        endif
        mov     ebx,26
inc_indirect:
        call    setflagsinc8
        inc     REGPC
        ret

        align 4
dec_b:
        dec     REGB
        jmp     dec_done
dec_c:
        dec     REGC
        jmp     dec_done
dec_d:
        mov     bx,REGDE
        inc     bh
        mov     REGDE,bx
        jmp     dec_done
dec_e:
        mov     bx,REGDE
        inc     bl
        mov     REGDE,bx
        jmp     dec_done
dec_h:
        mov     bx,REGHL
        inc     bh
        mov     REGHL,bx
        jmp     dec_done
dec_l:
        mov     bx,REGHL
        inc     bl
        mov     REGHL,bx
        jmp     dec_done
dec_a:
        dec     REGA
dec_done:
        mov     ebx,4
dec_set_flags:
        call    setflagsinc8
        or      REGF,02h        ; set n flag
        inc     REGPC
        ret
dec_hli:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        inc     byte ptr [ebx]
        ifdef   domemwrite
        call    check_memwrite
        endif
        mov     ebx,11
        jmp     dec_set_flags
dec_ixi:
        call    deref_ix_ebx
        jmp     do_dec
dec_iyi:
        call    deref_iy_ebx
do_dec:
        dec     byte ptr [ebx]
        ifdef   domemwrite
        call    check_memwrite
        endif
        mov     ebx,26
        jmp     dec_set_flags

        align 4
ld_b_c:
        mov     REGB,REGC
        jmp     doNOP
ld_b_d:
        mov     bx,REGDE
        mov     REGB,bh
        jmp     doNOP
ld_b_e:
        mov     bx,REGDE
        mov     REGB,bl
        jmp     doNOP
ld_b_h:
        mov     bx,REGHL
        mov     REGB,bh
        jmp     doNOP
ld_b_l:
        mov     bx,REGHL
        mov     REGB,bl
        jmp     doNOP
ld_b_a:
        mov     REGB,REGA
        jmp     doNOP

        align 4
ld_c_b:
        mov     REGC,REGB
        jmp     doNOP
ld_c_d:
        mov     bx,REGDE
        mov     REGC,bh
        jmp     doNOP
ld_c_e:
        mov     bx,REGDE
        mov     REGC,bl
        jmp     doNOP
ld_c_h:
        mov     bx,REGHL
        mov     REGC,bh
        jmp     doNOP
ld_c_l:
        mov     bx,REGHL
        mov     REGC,bl
        jmp     doNOP
ld_c_a:
        mov     REGC,REGA
        jmp     doNOP

        align 4
ld_d_b:
        mov     bx,REGDE
        mov     bh,REGB
        mov     REGDE,bx
        jmp     doNOP
ld_d_c:
        mov     bx,REGDE
        mov     bh,REGC
        mov     REGDE,bx
        jmp     doNOP
ld_d_e:
        mov     bx,REGDE
        mov     bh,bl
        mov     REGDE,bx
        jmp     doNOP
ld_d_h:
        mov     bx,REGDE
        mov     dx,REGHL
        mov     bh,dh
        mov     REGDE,bx
        jmp     doNOP
ld_d_l:
        mov     bx,REGDE
        mov     dx,REGHL
        mov     bh,dl
        mov     REGDE,bx
        jmp     doNOP
ld_d_a:
        mov     bx,REGDE
        mov     bh,REGA
        mov     REGDE,bx
        jmp     doNOP

        align 4
ld_e_b:
        mov     bx,REGDE
        mov     bl,REGB
        mov     REGDE,bx
        jmp     doNOP
ld_e_c:
        mov     bx,REGDE
        mov     bl,REGC
        mov     REGDE,bx
        jmp     doNOP
ld_e_d:
        mov     bx,REGDE
        mov     bl,bh
        mov     REGDE,bx
        jmp     doNOP
ld_e_h:
        mov     bx,REGDE
        mov     dx,REGHL
        mov     bl,dh
        mov     REGDE,bx
        jmp     doNOP
ld_e_l:
        mov     bx,REGDE
        mov     dx,REGHL
        mov     bl,dl
        mov     REGDE,bx
        jmp     doNOP
ld_e_a:
        mov     bx,REGDE
        mov     bl,REGA
        mov     REGDE,bx
        jmp     doNOP

        align 4
ld_h_b:
        mov     bx,REGHL
        mov     bh,REGB
        mov     REGHL,bx
        jmp     doNOP
ld_h_c:
        mov     bx,REGHL
        mov     bh,REGC
        mov     REGHL,bx
        jmp     doNOP
ld_h_d:
        mov     bx,REGHL
        mov     dx,REGDE
        mov     bh,dh
        mov     REGHL,bx
        jmp     doNOP
ld_h_e:
        mov     bx,REGHL
        mov     dx,REGDE
        mov     bh,dl
        mov     REGHL,bx
        jmp     doNOP
ld_h_l:
        mov     bx,REGHL
        mov     bh,bl
        mov     REGHL,bx
        jmp     doNOP
ld_h_a:
        mov     bx,REGHL
        mov     bh,REGA
        mov     REGHL,bx
        jmp     doNOP

        align 4
ld_l_b:
        mov     bx,REGHL
        mov     bl,REGB
        mov     REGHL,bx
        jmp     doNOP
ld_l_c:
        mov     bx,REGHL
        mov     bl,REGC
        mov     REGHL,bx
        jmp     doNOP
ld_l_d:
        mov     bx,REGHL
        mov     dx,REGDE
        mov     bl,dh
        mov     REGHL,bx
        jmp     doNOP
ld_l_e:
        mov     bx,REGHL
        mov     dx,REGDE
        mov     bl,dl
        mov     REGHL,bx
        jmp     doNOP
ld_l_h:
        mov     bx,REGHL
        mov     bl,bh
        mov     REGHL,bx
        jmp     doNOP
ld_l_a:
        mov     bx,REGHL
        mov     bl,REGA
        mov     REGHL,bx
        jmp     doNOP

        align 4
ld_a_b:
        mov     REGA,REGB
        jmp     doNOP
ld_a_c:
        mov     REGA,REGC
        jmp     doNOP
ld_a_d:
        mov     bx,REGDE
        mov     REGA,bh
        jmp     doNOP
ld_a_e:
        mov     bx,REGDE
        mov     REGA,bl
        jmp     doNOP
ld_a_h:
        mov     bx,REGHL
        mov     REGA,bh
        jmp     doNOP
ld_a_l:
        mov     bx,REGHL
        mov     REGA,bl
        jmp     doNOP

        align 4
ld_b_n:
        mov     REGB,byte ptr [REGPC+1]
        jmp     ld_r_n_done
ld_c_n:
        mov     REGC,byte ptr [REGPC+1]
        jmp     ld_r_n_done
ld_d_n:
        mov     bx,REGDE
        mov     bh,byte ptr [REGPC+1]
        mov     REGDE,bx
        jmp     ld_r_n_done
ld_e_n:
        mov     bx,REGDE
        mov     bl,byte ptr [REGPC+1]
        mov     REGDE,bx
        jmp     ld_r_n_done
ld_h_n:
        mov     bx,REGHL
        mov     bh,byte ptr [REGPC+1]
        mov     REGHL,bx
        jmp     ld_r_n_done
ld_l_n:
        mov     bx,REGHL
        mov     bl,byte ptr [REGPC+1]
        mov     REGHL,bx
        jmp     ld_r_n_done
ld_a_n:
        mov     REGA,byte ptr [REGPC+1]
ld_r_n_done:
        inc     REGPC
        inc     REGPC
        mov     ebx,7
        ret

        align 4
ld_b_hli:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        mov     REGB,byte ptr [ebx]
        jmp     ld_n_hli_done
ld_c_hli:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        mov     REGC,byte ptr [ebx]
        jmp     ld_n_hli_done
ld_d_hli:
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     bx,REGDE
        mov     bh,byte ptr [edx]
        mov     REGDE,bx
        jmp     ld_n_hli_done
ld_e_hli:
        mov     edx,dword ptr [_memstart]
        add     dx,REGHL
        mov     bx,REGDE
        mov     bl,byte ptr [edx]
        mov     REGDE,bx
        jmp     ld_n_hli_done
ld_h_hli:
        mov     edx,dword ptr [_memstart]
        add     dx,REGHL
        mov     bx,REGHL
        mov     bh,byte ptr [edx]
        mov     REGHL,bx
        jmp     ld_n_hli_done
ld_l_hli:
        mov     edx,dword ptr [_memstart]
        add     dx,REGHL
        mov     bx,REGHL
        mov     bl,byte ptr [edx]
        mov     REGHL,bx
        jmp     ld_n_hli_done
ld_a_hli:
        mov     edx,dword ptr [_memstart]
        add     dx,REGHL
        mov     REGA,byte ptr [edx]
ld_n_hli_done:
        inc     REGPC
        mov     ebx,7
        ret

        align 4
ld_a_i:
        mov     REGA,REGI
        jmp     ld_a_ir
ld_a_r:
        mov     REGA,REGR
ld_a_ir:
        and     REGF,01h       ; clear undesired flags
        mov     bl,REGA
        and     bl,028h         ; save x and y flag state
        or      REGF,bl
        or      REGA,REGA
        pushfd
        pop     ebx
        and     bl,0c0h         ; save s and z flag state
        or      REGF,bl
        cmp     byte ptr [_iff+1],0   ; check iff2
        jz      ld_a_ir_done
        or      REGF,04h        ; set parity flag
ld_a_ir_done:
        inc     REGPC
        mov     ebx,9
        ret
ld_i_a:
        mov     REGI,REGA
        jmp     ld_a_ir_done
ld_r_a:
        mov     REGR,REGA
        jmp     ld_a_ir_done

        align 4
ld_hli_b:
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     byte ptr [edx],REGB
        jmp     ld_hli_r_done
ld_hli_c:
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     byte ptr [edx],REGC
        jmp     ld_hli_r_done
ld_hli_d:
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     bx,REGDE
        mov     byte ptr [edx],bh
        jmp     ld_hli_r_done
ld_hli_e:
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     bx,REGDE
        mov     byte ptr [edx],bl
        jmp     ld_hli_r_done
ld_hli_h:
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     bx,REGHL
        mov     byte ptr [edx],bh
        jmp     ld_hli_r_done
ld_hli_l:
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     bx,REGHL
        mov     byte ptr [edx],bl
        jmp     ld_hli_r_done
ld_hli_a:
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     byte ptr [edx],REGA
ld_hli_r_done:
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        inc     REGPC
        mov     ebx,7
        ret

        align 4
ld_hli_n:
        mov     dl,byte ptr [REGPC + 1]
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        mov     byte ptr [ebx],dl
        ifdef   domemwrite
        call    check_memwrite
        endif
        inc     REGPC
        inc     REGPC
        mov     ebx,10
        ret

        align 4
ld_a_bci:
        movzx   edx,REGBC
        add     edx,dword ptr [_memstart]
        mov     REGA,byte ptr [edx]
        jmp     ld_a_done
ld_a_dei:
        movzx   edx,REGDE
        add     edx,dword ptr [_memstart]
        mov     REGA,byte ptr [edx]
ld_a_done:
        inc     REGPC
        mov     ebx,7
        ret
ld_a_ext:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     REGA,byte ptr [ebx]
        add     REGPC,3
        mov     ebx,13
        ret

        align 4
ld_hl_ext:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     REGHL,word ptr [ebx]
ld_hl_ext_done:
        add     REGPC,3
        mov     ebx,16
        ret
ld_ext_hl:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     word ptr [ebx],REGHL
        ifdef   domemwrite
        call    check_memwrite
        endif
        jmp     ld_hl_ext_done

        align 4
ld_bc_ext:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     REGBC,word ptr [ebx]
        jmp     ld_nn_ext_done
ld_de_ext:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     REGDE,word ptr [ebx]
        jmp     ld_nn_ext_done
ld_hl_ext2:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     REGBC,word ptr [ebx]
        jmp     ld_nn_ext_done
ld_sp_ext:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     bx,word ptr [ebx]
        mov     REGSP,bx
        jmp     ld_nn_ext_done
ld_ix_ext:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     bx,word ptr [ebx]
        mov     REGIX,bx
        jmp     ld_nn_ext_done
ld_iy_ext:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     bx,word ptr [ebx]
        mov     REGIY,bx
ld_nn_ext_done:
        add     REGPC,3
        mov     ebx,20
        ret

        align 4
ld_ext_bc:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     word ptr [ebx],REGBC
        jmp     ld_ext_nn_done
ld_ext_de:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     word ptr [ebx],REGDE
        jmp     ld_ext_nn_done
ld_ext_hl2:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     word ptr [ebx],REGHL
        jmp     ld_ext_nn_done
ld_ext_sp:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     dx,REGSP
        mov     word ptr [ebx],dx
        jmp     ld_ext_nn_done
ld_ext_ix:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     dx,REGIX
        mov     word ptr [ebx],dx
        jmp     ld_ext_nn_done
ld_ext_iy:
        movzx   ebx,word ptr [REGPC + 1]
        add     ebx,dword ptr [_memstart]
        mov     dx,REGIY
        mov     word ptr [ebx],dx
ld_ext_nn_done:
        ifdef   domemwrite
        call    check_memwrite
        endif
        add     REGPC,3
        mov     ebx,20
        ret

        align 4
ld_sp_hl:
        mov     REGSP,REGHL
        inc     REGPC
        mov     ebx,6
        ret
ld_sp_ix:
        mov     bx,REGIX
        jmp     ld_sp_done
ld_sp_iy:
        mov     bx,REGIY
ld_sp_done:
        mov     REGSP,bx
        inc     REGPC
        mov     ebx,10
        ret

        align 4
push_bc:
        mov     dx,REGBC
        jmp     push_finish
push_de:
        mov     dx,REGDE
        jmp     push_finish
push_hl:
        mov     dx,REGHL
        jmp     push_finish
push_af:
        mov     dx,REGAF
push_finish:
        call    push_dx 
        inc     REGPC
        mov     ebx,11
        ret

        align 4
push_ix:
        mov     dx,REGIX
        jmp     push_index_fin
push_iy:
        mov     dx,REGIY
push_index_fin:
        call    push_dx 
        inc     REGPC
        mov     ebx,15
        ret

        align 4
pop_bc:
        call    pop_bx
        mov     REGBC,bx
        jmp     pop_finish
pop_de:
        call    pop_bx
        mov     REGDE,bx
        jmp     pop_finish
pop_hl:
        call    pop_bx
        mov     REGHL,bx
        jmp     pop_finish
pop_af:
        call    pop_bx
        mov     REGAF,bx
pop_finish:
        inc     REGPC
        mov     ebx,10
        ret

        align 4
pop_ix:
        call    pop_bx
        mov     REGIX,bx
        jmp     pop_index_fin
pop_iy:
        call    pop_bx
        mov     REGIY,bx
pop_index_fin:
        inc     REGPC
        mov     ebx,14
        ret

        align 4
ex_de_hl:
        xchg    REGDE,REGHL
        jmp     ex_done
ex_af_afp:
        ror     eax,16
        jmp     ex_done
exx:
        ror     ecx,16      ; BC/BC'
        ror     edi,16      ; DE/DE'
        ror     esi,16      ; HL/HL'
ex_done:
        inc     REGPC
        mov     ebx,4
        ret
ex_spi_hl:
        movzx   ebx,REGSP
        add     ebx,dword ptr [_memstart]
        xchg    REGHL,word ptr [ebx]
        inc     REGPC
        mov     ebx,19
        ret

        align 4
ex_spi_ix:
        movzx   ebx,REGSP
        add     ebx,dword ptr [_memstart]
        mov     dx,REGIX
        xchg    dx,word ptr [ebx]
        mov     REGIX,dx
        jmp     ex_spi_index
ex_spi_iy:
        movzx   ebx,REGSP
        add     ebx,dword ptr [_memstart]
        mov     dx,REGIY
        xchg    dx,word ptr [ebx]
        mov     REGIY,dx
ex_spi_index:
        inc     REGPC
        mov     ebx,23
        ret

        align 4
jp_nz:
        test    REGF,040h
        jmp     jp_zero
jp_z:
        test    REGF,040h
        jmp     jp_not_zero
jp_nc:
        test    REGF,01h
        jmp     jp_zero
jp_c:
        test    REGF,01h
        jmp     jp_not_zero
jp_po:
        test    REGF,04h
        jmp     jp_zero
jp_pe:
        test    REGF,04h
        jmp     jp_not_zero
jp_p:
        test    REGF,080h
jp_zero:
        jz      jp_nn
        jmp     jp_nn_no
jp_m:
        test    REGF,080h
jp_not_zero:
        jz      jp_nn_no
jp_nn:
        movzx   REGPC,word ptr [REGPC+1]
        add     REGPC,dword ptr [_memstart]
        jmp     jp_nn_2
jp_nn_no:
        add     REGPC,3
jp_nn_2:
        mov     ebx,10
        ret
jp_hl:
        movzx   REGPC,REGHL
        mov     ebx,4
do_jp:
        add     REGPC,dword ptr [_memstart]
        ret
jp_ix:
        movzx   REGPC,REGIX
        mov     ebx,8
        jmp     do_jp
jp_iy:
        movzx   REGPC,REGIY
        mov     ebx,8
        jmp     do_jp

        align 4
jr_nz:
        test    REGF,040h
        jmp     jr_zero
jr_z:
        test    REGF,040h
        jmp     jr_not_zero
jr_nc:
        test    REGF,01h
jr_zero:
        jz      jr
        jmp     jr_no
jr_c:
        test    REGF,01h
jr_not_zero:
        jz      jr_no
jr:
        movsx   ebx,byte ptr [REGPC+1]
        inc     REGPC
        inc     REGPC
        add     REGPC,ebx
        mov     ebx,12
        ret
jr_no:
        inc     REGPC
        inc     REGPC
        mov     ebx,7
        ret
djnz:
        dec     REGB
        jz      djnz_exit
        movsx   ebx,byte ptr [REGPC+1]
        inc     REGPC
        inc     REGPC
        add     REGPC,ebx
        mov     ebx,13
        ret
djnz_exit:
        inc     REGPC
        inc     REGPC
        mov     ebx,10
        ret

        align 4
call_nz:
        test    REGF,040h
        jmp     call_zero
call_z:
        test    REGF,040h
        jmp     call_not_zero
call_nc:
        test    REGF,01h
        jmp     call_zero
call_c:
        test    REGF,01h
        jmp     call_not_zero
call_po:
        test    REGF,04h
        jmp     call_zero
call_pe:
        test    REGF,04h
        jmp     call_not_zero
call_p:
        test    REGF,080h
call_zero:
        jz      call_nn
        jmp     call_no
call_m:
        test    REGF,080h
call_not_zero:
        jz      call_no
call_nn:
        add     REGPC,3
        mov     edx,REGPC
        sub     edx,dword ptr [_memstart]
        call    push_dx 
        movzx   REGPC,word ptr [REGPC-2]
        add     REGPC,dword ptr [_memstart]
        mov     ebx,17
        ret
call_no:
        add     REGPC,3
        mov     ebx,10
        ret

        align 4
ret_nz:
        test    REGF,040h
        jmp     ret_zero
ret_z:
        test    REGF,040h
        jmp     ret_not_zero
ret_nc:
        test    REGF,01h
        jmp     ret_zero
ret_c:
        test    REGF,01h
        jmp     ret_not_zero
ret_po:
        test    REGF,04h
        jmp     ret_zero
ret_pe:
        test    REGF,04h
        jmp     ret_not_zero
ret_p:
        test    REGF,080h
ret_zero:
        jz      return_17
        jmp     ret_no
ret_m:
        test    REGF,080h
ret_not_zero:
        jz      ret_no
return_17:
        pushd   17
        jmp     do_ret
return:
        pushd   10
do_ret:
        call    pop_bx
        add     ebx,dword ptr [_memstart]
        mov     REGPC,ebx
        pop     ebx
        ret
ret_no:
        inc     REGPC
        mov     ebx,5
        ret

ld_b_ixi:
        call    deref_ix_ebx
        mov     REGB,byte ptr [ebx]
        jmp     ld_r_index
ld_b_iyi:
        call    deref_iy_ebx
        mov     REGB,byte ptr [ebx]
        jmp     ld_r_index
ld_c_ixi:
        call    deref_ix_ebx
        mov     REGC,byte ptr [ebx]
        jmp     ld_r_index
ld_c_iyi:
        call    deref_iy_ebx
        mov     REGC,byte ptr [ebx]
        jmp     ld_r_index
ld_d_ixi:
        call    deref_ix_ebx
        mov     dx,REGDE
        mov     dh,byte ptr [ebx]
        mov     REGDE,dx
        jmp     ld_r_index
ld_d_iyi:
        call    deref_iy_ebx
        mov     dx,REGDE
        mov     dh,byte ptr [ebx]
        mov     REGDE,dx
        jmp     ld_r_index
ld_e_ixi:
        call    deref_ix_ebx
        mov     dx,REGDE
        mov     dl,byte ptr [ebx]
        mov     REGDE,dx
        jmp     ld_r_index
ld_e_iyi:
        call    deref_iy_ebx
        mov     dx,REGDE
        mov     dl,byte ptr [ebx]
        mov     REGDE,dx
        jmp     ld_r_index
ld_h_ixi:
        call    deref_ix_ebx
        mov     dx,REGHL
        mov     dh,byte ptr [ebx]
        mov     REGHL,dx
        jmp     ld_r_index
ld_h_iyi:
        call    deref_iy_ebx
        mov     dx,REGHL
        mov     dh,byte ptr [ebx]
        mov     REGHL,dx
        jmp     ld_r_index
ld_l_ixi:
        call    deref_ix_ebx
        mov     dx,REGHL
        mov     dl,byte ptr [ebx]
        mov     REGHL,dx
        jmp     ld_r_index
ld_l_iyi:
        call    deref_iy_ebx
        mov     dx,REGHL
        mov     dl,byte ptr [ebx]
        mov     REGHL,dx
        jmp     ld_r_index
ld_a_ixi:
        call    deref_ix_ebx
        mov     REGA,byte ptr [ebx]
        jmp     ld_r_index
ld_a_iyi:
        call    deref_iy_ebx
        mov     REGA,byte ptr [ebx]
ld_r_index:
        inc     REGPC
        mov     ebx,19
        ret

        align 4
ld_ixi_b:
        call    deref_ix_ebx
        mov     byte ptr [ebx],REGB
        jmp     ld_index_r
ld_iyi_b:
        call    deref_iy_ebx
        mov     byte ptr [ebx],REGB
        jmp     ld_index_r
ld_ixi_c:
        call    deref_ix_ebx
        mov     byte ptr [ebx],REGC
        jmp     ld_index_r
ld_iyi_c:
        call    deref_iy_ebx
        mov     byte ptr [ebx],REGC
        jmp     ld_index_r
ld_ixi_d:
        call    deref_ix_ebx
        mov     dx,REGDE
        mov     byte ptr [ebx],dh
        jmp     ld_index_r
ld_iyi_d:
        call    deref_iy_ebx
        mov     dx,REGDE
        mov     byte ptr [ebx],dh
        jmp     ld_index_r
ld_ixi_e:
        call    deref_ix_ebx
        mov     dx,REGDE
        mov     byte ptr [ebx],dl
        jmp     ld_index_r
ld_iyi_e:
        call    deref_iy_ebx
        mov     dx,REGDE
        mov     byte ptr [ebx],dl
        jmp     ld_index_r
ld_ixi_h:
        call    deref_ix_ebx
        mov     dx,REGHL
        mov     byte ptr [ebx],dh
        jmp     ld_index_r
ld_iyi_h:
        call    deref_iy_ebx
        mov     dx,REGHL
        mov     byte ptr [ebx],dh
        jmp     ld_index_r
ld_ixi_l:
        call    deref_ix_ebx
        mov     dx,REGHL
        mov     byte ptr [ebx],dl
        jmp     ld_index_r
ld_iyi_l:
        call    deref_iy_ebx
        mov     dx,REGHL
        mov     byte ptr [ebx],dl
        jmp     ld_index_r
ld_ixi_a:
        call    deref_ix_ebx
        mov     byte ptr [ebx],REGA
        jmp     ld_index_r
ld_iyi_a:
        call    deref_iy_ebx
        mov     byte ptr [ebx],REGA
ld_index_r:
        ifdef   domemwrite
        call    check_memwrite
        endif
        mov     ebx,19
        inc     REGPC
        ret
ld_ixi_n:
        call    deref_ix_ebx
        mov     dl,byte ptr [REGPC+1]
        mov     byte ptr [ebx],dl
        jmp     ld_index_done
ld_iyi_n:
        call    deref_iy_ebx
        mov     dl,byte ptr [REGPC+1]
        mov     byte ptr [ebx],dl
ld_index_done:
        ifdef   domemwrite
        call    check_memwrite
        endif
        mov     ebx,19
        inc     REGPC
        inc     REGPC
        ret

        align 4
add_a_b:
        and     REGF,0feh
adc_a_b:
        mov     dl,REGB
        jmp     add_a_r_done
add_a_c:
        and     REGF,0feh
adc_a_c:
        mov     dl,REGC
        jmp     add_a_r_done
add_a_d:
        and     REGF,0feh
adc_a_d:
        mov     dx,REGDE
        mov     dl,dh
        jmp     add_a_r_done
add_a_e:
        and     REGF,0feh
adc_a_e:
        mov     dx,REGDE
        jmp     add_a_r_done
add_a_h:
        and     REGF,0feh
adc_a_h:
        mov     dx,REGHL
        mov     dl,dh
        jmp     add_a_r_done
add_a_l:
        and     REGF,0feh
adc_a_l:
        mov     dx,REGHL
        jmp     add_a_r_done
add_a_a:
        and     REGF,0feh
adc_a_a:
        mov     dl,REGA
add_a_r_done:
        mov     ebx,4
add_a_r_flags:
        bt      REGAF,0         ; get carry flag
        adc     REGA,dl
        pushfd
        mov     REGF,REGA
        and     REGF,028h       ; set x and y flags as necessary
        pop     edx
        and     dl,0d1h         ; get s, z, h and c flags
        and     dh,08h          ; get overflow flag
        shr     dh,1            ; move into correct position
        or      REGF,dl
        or      REGF,dh
        inc     REGPC
        ret
add_a_n:
        and     REGF,0feh
adc_a_n:
        inc     REGPC           ; another PC increment comes from the flags processing
        mov     dl,byte ptr [REGPC]
        mov     ebx,7
        jmp     add_a_r_flags
add_a_hli:
        and     REGF,0feh
adc_a_hli:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        mov     dl,byte ptr [ebx]
        mov     ebx,7
        jmp     add_a_r_flags
add_a_ixi:
        and     REGF,0feh
adc_a_ixi:
        call    deref_ix_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     add_a_r_flags
add_a_iyi:
        and     REGF,0feh
adc_a_iyi:
        call    deref_iy_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     add_a_r_flags

        align 4
sub_a_b:
        and     REGF,0feh
sbc_a_b:
        mov     dl,REGB
        jmp     sub_a_r_done
sub_a_c:
        and     REGF,0feh
sbc_a_c:
        mov     dl,REGC
        jmp     sub_a_r_done
sub_a_d:
        and     REGF,0feh
sbc_a_d:
        mov     dx,REGDE
        mov     dl,dh
        jmp     sub_a_r_done
sub_a_e:
        and     REGF,0feh
sbc_a_e:
        mov     dx,REGDE
        jmp     sub_a_r_done
sub_a_h:
        and     REGF,0feh
sbc_a_h:
        mov     dx,REGHL
        mov     dl,dh
        jmp     sub_a_r_done
sub_a_l:
        and     REGF,0feh
sbc_a_l:
        mov     dx,REGHL
        jmp     sub_a_r_done
sub_a_a:
        and     REGF,0feh
sbc_a_a:
        mov     dl,REGA
sub_a_r_done:
        mov     ebx,4
sub_a_r_flags:
        bt      REGAF,0
        sbb     REGA,dl
        pushfd
        mov     REGF,REGA
        and     REGF,028h       ; set x and y flags as necessary
        pop     edx
        and     dl,0d3h         ; get s, z, h, n and c flags
        and     dh,08h          ; get overflow flag
        shr     dh,1            ; move into correct position
        or      REGF,dl
        or      REGF,dh
        inc     REGPC
        ret
sub_a_n:
        and     REGF,0feh
sbc_a_n:
        inc     REGPC           ; another PC increment comes from the flags processing
        mov     dl,byte ptr [REGPC]
        mov     ebx,7
        jmp     sub_a_r_flags
sub_a_hli:
        and     REGF,0feh
sbc_a_hli:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        mov     dl,byte ptr [ebx]
        mov     ebx,7
        jmp     sub_a_r_flags
sub_a_ixi:
        and     REGF,0feh
sbc_a_ixi:
        call    deref_ix_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     sub_a_r_flags
sub_a_iyi:
        and     REGF,0feh
sbc_a_iyi:
        call    deref_iy_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     sub_a_r_flags

and_b:
        mov     dl,REGB
        jmp     and_done
and_c:
        mov     dl,REGC
        jmp     and_done
and_d:
        mov     bx,REGDE
        mov     dl,bh
        jmp     and_done
and_e:
        mov     dx,REGDE
        jmp     and_done
and_h:
        mov     bx,REGHL
        mov     dl,bh
        jmp     and_done
and_l:
        mov     dx,REGHL
        jmp     and_done
and_a:
        mov     dl,REGA
and_done:
        mov     ebx,4
and_set_flags:
        and     REGA,dl
        pushfd
        mov     REGF,REGA
        and     REGF,028h       ; get x and y flags
        or      REGF,010h       ; set half-carry flag
        pop     edx
        and     dl,0c4h
        or      REGF,dl
        inc     REGPC
        ret
and_n:
        mov     ebx,7
        inc     REGPC
        mov     dl,byte ptr [REGPC]
        jmp     and_set_flags
and_hli:
        mov     ebx,7
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     dl,byte ptr [edx]
        jmp     and_set_flags
and_ixi:
        call    deref_ix_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     and_set_flags
and_iyi:
        call    deref_iy_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     and_set_flags

        align 4
or_b:
        mov     dl,REGB
        jmp     or_done
or_c:
        mov     dl,REGC
        jmp     or_done
or_d:
        mov     bx,REGDE
        mov     dl,bh
        jmp     or_done
or_e:
        mov     dx,REGDE
        jmp     or_done
or_h:
        mov     bx,REGHL
        mov     dl,bh
        jmp     or_done
or_l:
        mov     dx,REGHL
        jmp     or_done
or_a:
        mov     dl,REGA
or_done:
        mov     ebx,4
or_set_flags:
        or      REGA,dl
        pushfd
        mov     REGF,REGA
        and     REGF,028h       ; get x and y flags
        pop     edx
        and     dl,0c4h
        or      REGF,dl
        inc     REGPC
        ret
or_n:
        mov     ebx,7
        inc     REGPC
        mov     dl,byte ptr [REGPC]
        jmp     or_set_flags
or_hli:
        mov     ebx,7
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     dl,byte ptr [edx]
        jmp     or_set_flags
or_ixi:
        call    deref_ix_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     or_set_flags
or_iyi:
        call    deref_iy_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     or_set_flags

        align 4
xor_b:
        mov     dl,REGB
        jmp     xor_done
xor_c:
        mov     dl,REGC
        jmp     xor_done
xor_d:
        mov     bx,REGDE
        mov     dl,bh
        jmp     xor_done
xor_e:
        mov     dx,REGDE
        jmp     xor_done
xor_h:
        mov     bx,REGHL
        mov     dl,bh
        jmp     xor_done
xor_l:
        mov     dx,REGHL
        jmp     xor_done
xor_a:
        mov     dl,REGA
xor_done:
        mov     ebx,4
xor_set_flags:
        xor     REGA,dl
        pushfd
        mov     REGF,REGA
        and     REGF,028h       ; get x and y flags
        pop     edx
        and     dl,0c4h
        or      REGF,dl
        inc     REGPC
        ret
xor_n:
        mov     ebx,7
        inc     REGPC
        mov     dl,byte ptr [REGPC]
        jmp     xor_set_flags
xor_hli:
        mov     ebx,7
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     dl,byte ptr [edx]
        jmp     xor_set_flags
xor_ixi:
        call    deref_ix_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     xor_set_flags
xor_iyi:
        call    deref_iy_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     xor_set_flags

        align 4
cp_b:
        mov     dl,REGB
        jmp     cp_done
cp_c:
        mov     dl,REGC
        jmp     cp_done
cp_d:
        mov     bx,REGDE
        mov     dl,bh
        jmp     cp_done
cp_e:
        mov     dx,REGDE
        jmp     cp_done
cp_h:
        mov     bx,REGHL
        mov     dl,bh
        jmp     cp_done
cp_l:
        mov     dx,REGHL
        jmp     cp_done
cp_a:
        mov     dl,REGA
cp_done:
        mov     ebx,4
cp_set_flags:
        mov     REGF,dl
        and     REGF,028h       ; get x and y flags
        cmp     REGA,dl
        pushfd
        pop     edx
        or      REGF,02h        ; set n flag
        and     dl,0d1h         ; save s, z, h and c flags
        or      REGF,dl
        and     dh,08h          ; get overflow flag
        shr     dh,1            ; move into correct position
        or      REGF,dh         ; set overflow flag if necessary
        inc     REGPC
        ret
cp_n:
        mov     ebx,7
        inc     REGPC
        mov     dl,byte ptr [REGPC]
        jmp     cp_set_flags
cp_hli:
        mov     ebx,7
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     dl,byte ptr [edx]
        jmp     cp_set_flags
cp_ixi:
        call    deref_ix_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     cp_set_flags
cp_iyi:
        call    deref_iy_ebx
        mov     dl,byte ptr [ebx]
        mov     ebx,19
        jmp     cp_set_flags

rep_set_flags:
        and     REGF,0c1h       ; save s, z and c flags only
        cmp     REGBC,0
        jz      set_parity_skip
        or      REGF,04h        ; set parity flag if not zero
set_parity_skip:
        add     dl,REGA         ; handle weird x and y flag situation
        and     dl,0ah
        shl     dl,2
        or      REGF,dl
        ret

        align 4
add_hl_bc:
        mov     dx,REGBC
        jmp     add_hl_done
add_hl_de:
        mov     dx,REGDE
        jmp     add_hl_done
add_hl_hl:
        mov     dx,REGHL
        jmp     add_hl_done
add_hl_sp:
        mov     dx,REGSP
add_hl_done:
        mov     ebx,11
do_16bit_add:
        push    ebx
        mov     bx,REGHL
        add     bl,dl
        adc     bh,dh
        mov     REGHL,bx
add16_set_flags:
        pushfd
        pop     edx
        and     REGF,0c4h       ; save s, z and p flags
        and     bh,028h
        or      REGF,bh
        and     dl,011h         ; get h and c flags
        or      REGF,dl
        pop     ebx
        inc     REGPC
        ret

        align 4
add_ix_bc:
        mov     dx,REGBC
        jmp     add_ix_done
add_ix_de:
        mov     dx,REGDE
        jmp     add_ix_done
add_ix_ix:
        mov     dx,REGIX
        jmp     add_ix_done
add_ix_sp:
        mov     dx,REGSP
add_ix_done:
        mov     bx,REGIX
        add     bl,dl
        adc     bh,dh
        mov     REGIX,bx
        pushd   15
        jmp     add16_set_flags

        align 4
add_iy_bc:
        mov     dx,REGBC
        jmp     add_iy_done
add_iy_de:
        mov     dx,REGDE
        jmp     add_iy_done
add_iy_iy:
        mov     dx,REGIY
        jmp     add_iy_done
add_iy_sp:
        mov     dx,REGSP
add_iy_done:
        mov     bx,REGIY
        add     bl,dl
        adc     bh,dh
        mov     REGIY,bx
        pushd   15
        jmp     add16_set_flags

        align 4
adc_hl_bc:
        mov     dx,REGBC
        jmp     adc_hl_done
adc_hl_de:
        mov     dx,REGDE
        jmp     adc_hl_done
adc_hl_hl:
        mov     dx,REGHL
        jmp     adc_hl_done
adc_hl_sp:
        mov     dx,REGSP
adc_hl_done:
        mov     bx,REGHL
        bt      REGAF,0         ; test emulated carry bit and set intel carry bit
adc_carry_done:
        adc     bx,dx
        mov     REGHL,bx
adc16_set_flags:
        pushfd
        pop     edx
        mov     REGF,bh
        and     REGF,028h       ; do x and y flags
        and     dl,091h         ; save c, h and s flags
        or      REGF,dl
        and     dh,08h          ; save overflow flag
        shr     dh,1            ; move into correct position
        or      REGF,dh
        mov     ebx,15
        inc     REGPC
        ret

        align 4
sbc_hl_bc:
        mov     dx,REGBC
        jmp     sbc_hl_done
sbc_hl_de:
        mov     dx,REGDE
        jmp     sbc_hl_done
sbc_hl_hl:
        mov     dx,REGHL
        jmp     sbc_hl_done
sbc_hl_sp:
        mov     dx,REGSP
sbc_hl_done:
        mov     bx,REGHL
        bt      REGAF,0         ; test emulated carry bit
sbc_carry_done:
        sbb     bx,dx
        mov     REGHL,bx
sbc16_set_flags:
        pushfd
        pop     edx
        mov     REGF,bh
        and     REGF,028h       ; do x and y flags
        or      REGF,02h        ; set n flag for subtraction
        and     dl,0d1h         ; save s, z, h and c flags
        or      REGF,dl
        and     dh,08h          ; save overflow flag
        shr     dh,1            ; move into correct position
        or      REGF,dh
        mov     ebx,15
        inc     REGPC
        ret

ldi:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        mov     dl,byte ptr [ebx]
        movzx   ebx,REGDE
        add     ebx,dword ptr [_memstart]
        mov     byte ptr [ebx],dl
        ifdef   domemwrite
        call    check_memwrite
        endif
        inc     REGDE
        inc     REGHL
        dec     REGBC
        ; set flags
        call    rep_set_flags
        inc     REGPC
        mov     ebx,16
        ret
ldir:
        call    ldi
        cmp     REGBC,0
        jz      ldir_done
        dec     REGPC           ; restart LDIR instruction
        dec     REGPC
        add     ebx,5           ; increase to 21 cycles
ldir_done:
        ret

ldd:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        mov     dl,byte ptr [ebx]
        movzx   ebx,REGDE
        add     ebx,dword ptr [_memstart]
        mov     byte ptr [ebx],dl
        ifdef   domemwrite
        call    check_memwrite
        endif
        dec     REGDE
        dec     REGHL
        dec     REGBC
        ; set flags
        call    rep_set_flags
        inc     REGPC
        mov     ebx,16
        ret
lddr:
        call    ldd
        cmp     REGBC,0
        jz      lddr_done
        dec     REGPC           ; restart LDIR instruction
        dec     REGPC
        add     ebx,5           ; increase to 21 cycles
lddr_done:
        ret

cpi:
        push    REGAF
        call    cp_hli          ; also causes a REGPC increment
        pop     bx
        and     REGF,0fah       ; clear parity and carry flags
        and     bl,01h          ; get carry flag before operation
        or      REGF,bl
        inc     REGHL
        dec     REGBC
        jz      cpi_skip_setbit
        or      REGF,04h        ; set parity flag
cpi_skip_setbit:
        mov     ebx,16
        ret
cpir:
        call    cpi             ; does a REGPC increment
        add     ebx,2
        test    REGF,040h       ; test zero flag for equality
        jnz     cpir_done
        cmp     REGBC,0         ; test for completion
        je      cpir_done
        dec     REGPC
        dec     REGPC
        add     ebx,3
cpir_done:
        ret

cpd:
        push    REGAF
        call    cp_hli          ; also causes a REGPC increment
        pop     bx
        and     REGF,0fah       ; clear parity and carry flags
        and     bl,01h          ; get carry flag before operation
        or      REGF,bl
        dec     REGHL
        dec     REGBC
        jz      cpd_skip_setbit
        or      REGF,04h        ; set parity flag
cpd_skip_setbit:
        mov     ebx,16
        ret
cpdr:
        call    cpd             ; does a REGPC increment
        add     ebx,2
        test    REGF,040h       ; test zero flag for equality
        jnz     cpdr_done
        cmp     REGBC,0         ; test for completion
        je      cpdr_done
        add     ebx,3
        dec     REGPC
        dec     REGPC
cpdr_done:
        ret

do_daa:
        push    REGAF
        xchg    REGA,REGF       ; value must be in al to do daa
        xor     ebx,ebx
        pop     bx              ; restore flags
        mov     bh,0
        push    ebx
        popfd                   ; set cpu flags
        daa
        pushfd
        pop     ebx             ; get cpu flags
        xchg    REGA,REGF       ; get correct values again
        mov     dl,REGA
        and     dl,028h
        and     REGF,02h
        or      REGF,dl
        and     bl,0d5h         ; save s, z, h, p, c flags from intel cpu
        or      REGF,bl
        mov     ebx,4
        inc     REGPC
        ret

cpl:
        not     REGA
        mov     bl,REGA
        and     bl,028h       ; get x and y flags
        and     REGF,0c5h     ; save s, z, p, c flags
        or      REGF,012h     ; set h and n flags
        or      REGF,bl
        mov     ebx,4
        inc     REGPC
        ret

do_neg:
        neg     REGA
        pushfd
        mov     REGF,REGA
        and     REGF,028h     ; set x and y flags as necessary
        pop     ebx
        and     bl,0d5h       ; get s, z, h, p and c flags
        or      REGF,bl
        or      REGF,02h      ; set n flag
        mov     ebx,8
        inc     REGPC
        ret

ccf:
        mov     bx,REGAF
        xor     REGF,01h      ; complement carry flag
        and     REGF,0c5h     ; save s, z, p and c flags
        and     bh,028h
        shl     bl,4
        and     bl,010h
        or      REGF,bh
        or      REGF,bl
        mov     ebx,4
        inc     REGPC
        ret

scf:
        or      REGF,01h      ; set carry flag
        and     REGF,0c5h     ; save s, z, p and c flags
        mov     bl,REGA
        and     bl,028h
        or      REGF,bl       ; set x and y flags as necessary
        mov     ebx,4
        inc     REGPC
        ret

do_halt:
        mov     byte ptr [_halted],1            ; Set CPU to halted mode
        jmp     doNOP                           ; this will increment PC

do_di:
        mov     byte ptr [_intdelay],0
        mov     word ptr [_iff],0
        mov     ebx,4
        inc     REGPC
        ret
ei:
        mov     byte ptr [_intdelay],2      ; 2 instruction delay (this and the next)
        mov     ebx,4
        inc     REGPC
        ret
im0:
        mov     byte ptr [_inttype],0
        jmp     imdone
im1:
        mov     byte ptr [_inttype],1
        jmp     imdone
im2:
        mov     byte ptr [_inttype],2
imdone:
        mov     ebx,8
        inc     REGPC
        ret

do_reti:
do_retn:
        call    pop_bx
        movzx   REGPC,bx
        add     REGPC,dword ptr [_memstart]
        mov     bl,byte ptr [_iff+1]
        mov     byte ptr [_iff],bl
        mov     ebx,14
        ret

rst:
        mov     edx,REGPC
        sub     edx,dword ptr [_memstart]
        inc     edx
        call    push_dx
        xor     ebx,ebx
        mov     bl,byte ptr [esp+4]         ; get opcode
        and     bl,038h                     ; mask off address
        add     ebx,dword ptr [_memstart]
        mov     REGPC,ebx
        mov     ebx,11
        ret

; port to read in bx
; byte read in bl
; destroys edx and ebx
; does not set any REGF flags
read_byte:
        xor     edx,edx
        cmp     dword ptr [_ioreadproc],0
        je      read_byte_done
        pushad
        push    ebx
        call    dword ptr [_ioreadproc]
        add     esp,4
        mov     byte ptr [esp+20],al        ; copy into saved dl on stack
        popad
read_byte_done:
        mov     bl,dl
        ret

; port to write in bx
; byte to write in dl
; destroys edx and ebx
; does not set any REGF flags
write_byte:
        cmp     dword ptr [_iowriteproc],0
        je      write_byte_done
        pushad
        push    edx     ; push byte
        push    ebx     ; push port
        call    dword ptr [_iowriteproc]
        add     esp,8
        popad
write_byte_done:
        ret

in_a_n:
        mov     bh,REGA
        mov     bl,byte ptr [REGPC+1]
        call    read_byte
        mov     REGA,bl
        add     REGPC,2
        mov     ebx,11
        ret

in_r_c:
        mov     bx,REGBC
        call    read_byte
        mov     bh,byte ptr [REGPC]
        cmp     bh,078h
        je      in_a_c
        cmp     bh,070h
        je      in_f_c
        cmp     bh,068h
        je      in_l_c
        cmp     bh,060h
        je      in_h_c
        cmp     bh,058h
        je      in_e_c
        cmp     bh,050h
        je      in_d_c
        cmp     bh,048h
        je      in_c_c
in_b_c:
        mov     REGB,bl
        jmp     in_r_c_done
in_c_c:
        mov     REGC,bl
        jmp     in_r_c_done
in_d_c:
        mov     dx,REGDE
        mov     dh,bl
        mov     REGDE,dx
        jmp     in_r_c_done
in_e_c:
        mov     dx,REGDE
        mov     dl,bl
        mov     REGDE,dx
        jmp     in_r_c_done
in_h_c:
        mov     dx,REGHL
        mov     dh,bl
        mov     REGHL,dx
        jmp     in_r_c_done
in_l_c:
        mov     dx,REGHL
        mov     dl,bl
        mov     REGHL,dx
        jmp     in_r_c_done
in_f_c:
        mov     REGF,bl
        jmp     in_r_c_done
in_a_c:
        mov     REGA,bl
in_r_c_done:
        or      bl,bl                   ; set flags from byte read
        pushfd
        pop     edx
        and     bl,028h
        and     REGF,01h
        or      REGF,bl
        and     dl,0c4h
        or      REGF,dl
        mov     ebx,12
        inc     REGPC
        ret

ini:
        mov     bx,REGBC
        call    read_byte
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        push    ebx
        mov     ebx,edx
        call    check_memwrite
        pop     ebx
        endif
        inc     REGHL
        dec     REGB
        pushfd
        mov     REGF,REGB
        and     REGF,028h
        pop     edx
        and     dl,0c4h
        or      REGF,dl
        mov     dl,REGC
        inc     dl
        add     dl,bl
        jnc     ini_setcarry_skip
        or      REGF,011h
ini_setcarry_skip:
        mov     ebx,16
        inc     REGPC
        ret

inir:
        call    ini
        test    REGF,040h
        jnz     inir_done
        dec     REGPC
        dec     REGPC
        ret
inir_done:
        mov     ebx,21
        ret

ind:
        mov     bx,REGBC
        call    read_byte
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        push    ebx
        mov     ebx,edx
        call    check_memwrite
        pop     ebx
        endif
        dec     REGHL
        dec     REGB
        pushfd
        mov     REGF,REGB
        and     REGF,028h
        pop     edx
        and     dl,0c4h
        or      REGF,dl
        mov     dl,REGC
        dec     dl
        add     dl,bl
        jnc     ind_setcarry_skip
        or      REGF,011h
ind_setcarry_skip:
        mov     ebx,16
        inc     REGPC
        ret

indr:
        call    ind
        test    REGF,040h
        jnz     indr_done
        dec     REGPC           ; restart this instruction
        dec     REGPC
        ret
indr_done:
        mov     ebx,21
        ret

out_n_a:
        mov     bh,REGA
        mov     bl,byte ptr [REGPC+1]
        mov     dl,REGA
        call    write_byte
        mov     ebx,11
        inc     REGPC
        inc     REGPC
        ret

out_c_b:
        mov     dl,REGB
        jmp     out_c_done
out_c_c:
        mov     dl,REGC
        jmp     out_c_done
out_c_d:
        mov     dx,REGDE
        mov     dl,dh
        jmp     out_c_done
out_c_e:
        mov     dx,REGDE
        jmp     out_c_done
out_c_h:
        mov     dx,REGHL
        mov     dl,dh
        jmp     out_c_done
out_c_l:
        mov     dx,REGHL
        jmp     out_c_done
out_c_0:
        mov     dl,0
        jmp     out_c_done
out_c_a:
        mov     dl,REGA
out_c_done:
        mov     bx,REGBC
        call    write_byte
        mov     ebx,12
        inc     REGPC
        ret

outi:
        mov     bx,REGBC
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     dl,byte ptr [edx]
        call    write_byte
        inc     REGHL
        dec     REGB
        pushfd
        mov     REGF,REGB
        and     REGF,028h
        pop     edx
        and     dl,0c4h
        or      REGF,dl
        mov     dx,REGHL
        add     dl,bl
        jnc     outd_setcarry_skip
        or      REGF,011h
outi_setcarry_skip:
        mov     ebx,16
        inc     REGPC
        ret

otir:
        call    outi
        test    REGF,040h
        jnz     otir_done
        dec     REGPC
        dec     REGPC
        ret
otir_done:
        mov     ebx,21
        ret

outd:
        mov     bx,REGBC
        movzx   edx,REGHL
        add     edx,dword ptr [_memstart]
        mov     dl,byte ptr [edx]
        call    write_byte
        dec     REGHL
        dec     REGB
        pushfd
        mov     REGF,REGB
        and     REGF,028h
        pop     edx
        and     dl,0c4h
        or      REGF,dl
        mov     dx,REGHL
        add     dl,bl
        jnc     outd_setcarry_skip
        or      REGF,011h
outd_setcarry_skip:
        mov     ebx,16
        inc     REGPC
        ret

otdr:
        call    outd
        test    REGF,040h
        jnz     otdr_done
        dec     REGPC
        dec     REGPC
        ret
otdr_done:
        mov     ebx,21
        ret

        align 4
rlca:
        rol     REGA,1
        and     REGF,0c4h       ; save s, z and p flags
        mov     bl,REGA
        and     bl,029h         ; get x, y and c flags from a
        or      REGF,bl
rotate_ret:
        mov     ebx,4
        inc     REGPC
        ret
rla:
        rol     REGA,1
        mov     bl,REGA
        and     REGA,0feh       ; clear low bit from a
        mov     bh,REGF
        and     bh,01h
        or      REGA,bh
        and     REGF,0c4h       ; save s, z and p flags
        and     bl,029h         ; get x, y and c flags from a
        or      REGF,bl
        jmp     rotate_ret
rrca:
        mov     bl,REGA
        ror     REGA,1
        and     REGF,0c4h       ; save s, z and p flags
        and     bl,01h
        or      REGF,bl         ; set carry flag as necessary
        mov     bl,REGA
        and     bl,028h         ; get x and y flags from a
        or      REGF,bl         ; set x and y flags as necessary
        jmp     rotate_ret
rra:
        mov     bl,REGA
        ror     REGA,1
        and     REGA,07fh       ; clear highest bit
        mov     bh,REGF
        and     bh,01h          ; get carry bit
        ror     bh,1            ; move carry bit to high order
        or      REGA,bh
        and     bl,01h
        and     REGF,0c4h       ; save s, z and p flags
        or      REGF,bl         ; set c flag as necessary
        mov     bl,REGA
        and     bl,028h         ; get x and y flags from a
        or      REGF,bl
        jmp     rotate_ret

rlc_b:
        rol     REGB,1
        mov     bl,REGB
        jmp     rlc_done
rlc_c:
        rol     REGC,1
        mov     bl,REGC
        jmp     rlc_done
rlc_d:
        mov     bx,REGDE
        rol     bh,1
        mov     REGDE,bx
        mov     bl,bh
        jmp     rlc_done
rlc_e:
        mov     bx,REGDE
        rol     bl,1
        mov     REGDE,bx
        jmp     rlc_done
rlc_h:
        mov     bx,REGHL
        rol     bh,1
        mov     REGHL,bx
        mov     bl,bh
        jmp     rlc_done
rlc_l:
        mov     bx,REGHL
        rol     bl,1
        mov     REGHL,bx
        jmp     rlc_done
rlc_a:
        rol     REGA,1
        mov     bl,REGA
rlc_done:
        mov     edx,8
rlc_set_flags:
        mov     REGF,bl
        and     REGF,029h       ; set x, y and c flags from operation
        or      bl,bl           ; set flags from operation
        pushfd
        pop     ebx
        and     bl,0c4h         ; get s, z, and p flags from operation
        or      REGF,bl
rlc_ret:
        inc     REGPC
        mov     ebx,edx
        ret
rlc_ind:
        rol     byte ptr [ebx],1
        ifdef   domemwrite
        call    check_memwrite
        endif
        mov     bl,byte ptr [ebx]
        mov     edx,15
        jmp     rlc_set_flags

rrc_b:
        ror     REGB,1
        mov     bl,REGB
        jmp     rrc_done
rrc_c:
        ror     REGC,1
        mov     bl,REGC
        jmp     rrc_done
rrc_d:
        mov     bx,REGDE
        ror     bh,1
        mov     REGDE,bx
        mov     bl,bh
        jmp     rrc_done
rrc_e:
        mov     bx,REGDE
        ror     bl,1
        mov     REGDE,bx
        jmp     rrc_done
rrc_h:
        mov     bx,REGHL
        ror     bh,1
        mov     REGHL,bx
        mov     bl,bh
        jmp     rrc_done
rrc_l:
        mov     bx,REGHL
        ror     bl,1
        mov     REGHL,bx
        jmp     rrc_done
rrc_a:
        ror     REGA,1
        mov     bl,REGA
rrc_done:
        mov     edx,8
rrc_set_flags:
        mov     REGF,bl
        and     REGF,028h       ; set x and y flags from operation
        or      bl,bl           ; set flags from operation
        pushfd
        and     bl,080h         ; get c flag from operation
        rol     bl,1            ; position c flag in correct location
        or      REGF,bl
        pop     ebx
        and     bl,0c4h         ; get s, z, and p flags from operation
        or      REGF,bl
rrc_ret:
        inc     REGPC
        mov     ebx,edx
        ret
rrc_ind:
        ror     byte ptr [ebx],1
        ifdef   domemwrite
        call    check_memwrite
        endif
        mov     bl,byte ptr [ebx]
        mov     edx,15
        jmp     rrc_set_flags

        align 4
rl_b:
        mov     bl,REGB
        call    do_rl
        mov     REGB,bl
        jmp     rl_end
rl_c:
        mov     bl,REGC
        call    do_rl
        mov     REGC,bl
        jmp     rl_end
rl_d:
        mov     bx,REGDE
        xchg    bl,bh
        call    do_rl
        xchg    bl,bh
        mov     REGDE,bx
        jmp     rl_end
rl_e:
        mov     bx,REGDE
        call    do_rl
        mov     REGDE,bx
        jmp     rl_end
rl_h:
        mov     bx,REGHL
        xchg    bl,bh
        call    do_rl
        xchg    bl,bh
        mov     REGHL,bx
        jmp     rl_end
rl_l:
        mov     bx,REGHL
        call    do_rl
        mov     REGHL,bx
        jmp     rl_end
rl_a:
        mov     bl,REGA
        call    do_rl
        mov     REGA,bl
rl_end:
        inc     REGPC
        mov     ebx,8
        ret
rl_ind:
        push    ebx
        mov     bl,byte ptr [ebx]
        call    do_rl
        pop     edx
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        inc     REGPC
        mov     ebx,15
        ret
do_rl:
        rol     bl,1
        mov     dh,bl
        and     bl,0feh         ; clear low bit
        mov     dl,REGF
        and     dl,01h
        or      bl,dl
        mov     REGF,dh
        and     REGF,029h       ; set x, y and c bits as necessary
        or      bl,bl
        pushfd
        pop     edx
        and     dl,0c4h         ; get s, z and p flags
        or      REGF,dl
        ret

        align 4
rr_b:
        mov     bl,REGB
        call    do_rr
        mov     REGB,bl
        jmp     rr_end
rr_c:
        mov     bl,REGC
        call    do_rr
        mov     REGC,bl
        jmp     rr_end
rr_d:
        mov     bx,REGDE
        xchg    bl,bh
        call    do_rr
        xchg    bl,bh
        mov     REGDE,bx
        jmp     rr_end
rr_e:
        mov     bx,REGDE
        call    do_rr
        mov     REGDE,bx
        jmp     rr_end
rr_h:
        mov     bx,REGHL
        xchg    bl,bh
        call    do_rr
        xchg    bl,bh
        mov     REGHL,bx
        jmp     rr_end
rr_l:
        mov     bx,REGHL
        call    do_rr
        mov     REGHL,bx
        jmp     rr_end
rr_a:
        mov     bl,REGA
        call    do_rr
        mov     REGA,bl
rr_end:
        inc     REGPC
        mov     ebx,8
        ret
rr_ind:
        push    ebx
        mov     bl,byte ptr [ebx]
        call    do_rr
        pop     edx
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        inc     REGPC
        mov     ebx,15
        ret
do_rr:
        mov     dh,bl
        ror     bl,1
        and     bl,07fh         ; clear high bit
        mov     dl,REGF
        and     dl,01h
        ror     dl,1            ; position c flag in correct location
        or      bl,dl
        and     dh,01h          ; get new c flag
        mov     REGF,dh
        mov     dh,bl
        and     dh,028h        ; set x and y bits as necessary
        or      REGF,dh
        or      bl,bl
        pushfd
        pop     edx
        and     dl,0c4h         ; get s, z and p flags
        or      REGF,dl
        ret

sla_b:
        mov     bl,REGB
        call    do_sla
        mov     REGB,bl
        jmp     sla_end
sla_c:
        mov     bl,REGC
        call    do_sla
        mov     REGC,bl
        jmp     sla_end
sla_d:
        mov     bx,REGDE
        xchg    bh,bl
        call    do_sla
        xchg    bh,bl
        mov     REGDE,bx
        jmp     sla_end
sla_e:
        mov     bx,REGDE
        call    do_sla
        mov     REGDE,bx
        jmp     sla_end
sla_h:
        mov     bx,REGHL
        xchg    bh,bl
        call    do_sla
        xchg    bh,bl
        mov     REGHL,bx
        jmp     sla_end
sla_l:
        mov     bx,REGHL
        call    do_sla
        mov     REGHL,bx
        jmp     sla_end
sla_a:
        mov     bl,REGA
        call    do_sla
        mov     REGA,bl
sla_end:
        mov     ebx,8
        inc     REGPC
        ret
sla_ind:
        push    ebx
        mov     bl,byte ptr [ebx]
        call    do_sla
        pop     edx
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        inc     REGPC
        mov     ebx,15
        ret
do_sla:
        rol     bl,1
        mov     REGF,bl
        and     REGF,029h       ; get x, y and c flags
        and     bl,0feh         ; clear lsb
        pushfd
        pop     edx
        and     dl,0c4h         ; save s, z and p flags
        or      REGF,dl
        ret

sll_b:
        mov     bl,REGB
        call    do_sll
        mov     REGB,bl
        jmp     sll_end
sll_c:
        mov     bl,REGC
        call    do_sll
        mov     REGC,bl
        jmp     sll_end
sll_d:
        mov     bx,REGDE
        xchg    bh,bl
        call    do_sll
        xchg    bh,bl
        mov     REGDE,bx
        jmp     sll_end
sll_e:
        mov     bx,REGDE
        call    do_sll
        mov     REGDE,bx
        jmp     sll_end
sll_h:
        mov     bx,REGHL
        xchg    bh,bl
        call    do_sll
        xchg    bh,bl
        mov     REGHL,bx
        jmp     sll_end
sll_l:
        mov     bx,REGHL
        call    do_sll
        mov     REGHL,bx
        jmp     sll_end
sll_a:
        mov     bl,REGA
        call    do_sll
        mov     REGA,bl
sll_end:
        mov     ebx,8
        inc     REGPC
        ret
sll_ind:
        push    ebx
        mov     bl,byte ptr [ebx]
        call    do_sla
        pop     edx
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        inc     REGPC
        mov     ebx,15
        ret
do_sll:
        rol     bl,1
        mov     REGF,bl
        and     REGF,029h       ; get x, y and c flags
        or      bl,01h          ; set lsb
        pushfd
        pop     edx
        and     dl,0c4h         ; save s, z and p flags
        or      REGF,dl
        ret

sra_b:
        mov     bl,REGB
        call    do_sra
        mov     REGB,bl
        jmp     sra_end
sra_c:
        mov     bl,REGC
        call    do_sra
        mov     REGC,bl
        jmp     sra_end
sra_d:
        mov     bx,REGDE
        xchg    bh,bl
        call    do_sra
        xchg    bh,bl
        mov     REGDE,bx
        jmp     sra_end
sra_e:
        mov     bx,REGDE
        call    do_sra
        mov     REGDE,bx
        jmp     sra_end
sra_h:
        mov     bx,REGHL
        xchg    bh,bl
        call    do_sra
        xchg    bh,bl
        mov     REGHL,bx
        jmp     sra_end
sra_l:
        mov     bx,REGHL
        call    do_sra
        mov     REGHL,bx
        jmp     sra_end
sra_a:
        mov     bl,REGA
        call    do_sra
        mov     REGA,bl
sra_end:
        mov     ebx,8
        inc     REGPC
        ret
sra_ind:
        push    ebx
        mov     bl,byte ptr [ebx]
        call    do_sra
        pop     edx
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        inc     REGPC
        mov     ebx,15
        ret
do_sra:
        mov     dl,bl
        sar     bl,1
        mov     REGF,dl
        and     REGF,01h        ; set c flag
        mov     dl,bl
        and     dl,028h         ; get x and y flags
        or      REGF,dl         ; set x and y flags as necessary
        or      bl,bl
        pushfd
        pop     edx
        and     dl,0c4h         ; save s, z and p flags
        or      REGF,dl
        ret

        align 4
srl_b:
        mov     bl,REGB
        call    do_srl
        mov     REGB,bl
        jmp     srl_end
srl_c:
        mov     bl,REGC
        call    do_srl
        mov     REGC,bl
        jmp     srl_end
srl_d:
        mov     bx,REGDE
        xchg    bh,bl
        call    do_srl
        xchg    bh,bl
        mov     REGDE,bx
        jmp     srl_end
srl_e:
        mov     bx,REGDE
        call    do_srl
        mov     REGDE,bx
        jmp     srl_end
srl_h:
        mov     bx,REGHL
        xchg    bh,bl
        call    do_srl
        xchg    bh,bl
        mov     REGHL,bx
        jmp     srl_end
srl_l:
        mov     bx,REGHL
        call    do_srl
        mov     REGHL,bx
        jmp     srl_end
srl_a:
        mov     bl,REGA
        call    do_srl
        mov     REGA,bl
srl_end:
        mov     ebx,8
        inc     REGPC
        ret
srl_ind:
        push    ebx
        mov     bl,byte ptr [ebx]
        call    do_srl
        pop     edx
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        inc     REGPC
        mov     ebx,15
        ret
do_srl:
        mov     dl,bl
        shr     bl,1
        mov     REGF,dl
        and     REGF,01h        ; set c flag
        mov     dl,bl
        and     dl,028h         ; get x and y flags
        or      REGF,dl         ; set x and y flags as necessary
        or      bl,bl
        pushfd
        pop     edx
        and     dl,0c4h         ; save s, z and p flags
        or      REGF,dl
        ret

        align 4
rld:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        rol     byte ptr [ebx],4
        mov     dl,byte ptr [ebx]
        and     byte ptr [ebx],0f0h
        mov     dh,REGA
        and     dh,0fh
        or      byte ptr [ebx],dh
        ifdef   domemwrite
        call    check_memwrite
        endif
        and     REGA,0f0h
        and     dl,0fh
        or      REGA,dl
rxd_setflags:
        pushfd
        and     REGF,01h
        mov     dl,REGA
        and     dl,028h
        or      REGF,dl
        pop     ebx
        and     bl,0c4h
        or      REGF,bl
        inc     REGPC
        mov     ebx,18
        ret
rrd:
        movzx   ebx,REGHL
        add     ebx,dword ptr [_memstart]
        mov     dl,byte ptr [ebx]
        and     byte ptr [ebx],0f0h
        mov     dh,REGA
        and     dh,0fh
        or      byte ptr [ebx],dh
        ror     byte ptr [ebx],4
        ifdef   domemwrite
        call    check_memwrite
        endif
        and     REGA,0f0h
        and     dl,0fh
        or      REGA,dl
        jmp     rxd_setflags

        align 4
bit_b:
        mov     bl,REGB
        jmp     bit_done
bit_c:
        mov     bl,REGC
        jmp     bit_done
bit_d:
        mov     bx,REGDE
        mov     bl,bh
        jmp     bit_done
bit_e:
        mov     bx,REGDE
        jmp     bit_done
bit_h:
        mov     bx,REGHL
        mov     bl,bh
        jmp     bit_done
bit_l:
        mov     bx,REGHL
        jmp     bit_done
bit_a:
        mov     bl,REGA
bit_done:
        mov     edx,8
        push    edx
do_bit:
        mov     dl,byte ptr [esp+8]     ; instruction parameter
        shr     dl,3
        and     dl,07h
        mov     dh,01h                  ; start with bit 0
bit_loop:
        cmp     dl,0
        je      bit_loop_done
        dec     dl
        shl     dh,1
        jmp     bit_loop
bit_loop_done:
        and     bl,dh
        pushfd
        pop     edx
        and     bl,028h
        and     REGF,01h                ; retain only the carry flag
        or      REGF,bl                 ; set x and y flag as necessary
        or      REGF,010h               ; set h flag
        and     dl,0c4h                 ; get s, f and p flags
        or      REGF,dl
        inc     REGPC
        pop     ebx
        ret
bit_ind:
        mov     bl,byte ptr [ebx]
        mov     edx,12
        push    edx
        jmp     do_bit

        align 4
set_b:
        mov     bl,REGB
        call    set_bit
        mov     REGB,bl
        jmp     set_done
set_c:
        mov     bl,REGC
        call    set_bit
        mov     REGC,bl
        jmp     set_done
set_d:
        mov     bx,REGDE
        xchg    bl,bh
        call    set_bit
        xchg    bl,bh
        mov     REGDE,bx
        jmp     set_done
set_e:
        mov     bx,REGDE
        call    set_bit
        mov     REGDE,bx
        jmp     set_done
set_h:
        mov     bx,REGHL
        xchg    bl,bh
        call    set_bit
        xchg    bl,bh
        mov     REGHL,bx
        jmp     set_done
set_l:
        mov     bx,REGHL
        call    set_bit
        mov     REGHL,bx
        jmp     set_done
set_a:
        mov     bl,REGA
        call    set_bit
        mov     REGA,bl
set_done:
        mov     ebx,8
        inc     REGPC
        ret
set_ind:
        mov     edx,dword ptr [esp+4]
        push    edx
        push    ebx
        mov     bl,byte ptr [ebx]
        call    set_bit
        pop     edx
        add     esp,4
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        mov     ebx,15
        inc     REGPC
        ret
set_bit:
        mov     dl,byte ptr [esp+8]
        shr     dl,3
        and     dl,07h
        mov     dh,1
set_loop:
        cmp     dl,0
        je      set_loop_done
        dec     dl
        shl     dh,1
        jmp     set_loop
set_loop_done:
        or      bl,dh
        ret

        align 4
res_b:
        mov     bl,REGB
        call    res_bit
        mov     REGB,bl
        jmp     res_done
res_c:
        mov     bl,REGC
        call    res_bit
        mov     REGC,bl
        jmp     res_done
res_d:
        mov     bx,REGDE
        xchg    bl,bh
        call    res_bit
        xchg    bl,bh
        mov     REGDE,bx
        jmp     res_done
res_e:
        mov     bx,REGDE
        call    res_bit
        mov     REGDE,bx
        jmp     res_done
res_h:
        mov     bx,REGHL
        xchg    bl,bh
        call    res_bit
        xchg    bl,bh
        mov     REGHL,bx
        jmp     res_done
res_l:
        mov     bx,REGHL
        call    res_bit
        mov     REGHL,bx
        jmp     res_done
res_a:
        mov     bl,REGA
        call    res_bit
        mov     REGA,bl
res_done:
        mov     ebx,8
        inc     REGPC
        ret
res_ind:
        mov     edx,dword ptr [esp+4]
        push    edx             ; push copy of instruction
        push    ebx             ; save pointer to mem loc
        mov     bl,byte ptr [ebx]
        call    res_bit
        pop     edx
        add     esp,4
        mov     byte ptr [edx],bl
        ifdef   domemwrite
        mov     ebx,edx
        call    check_memwrite
        endif
        mov     ebx,15
        inc     REGPC
        ret
res_bit:
        mov     dl,byte ptr [esp+8]
        shr     dl,3
        and     dl,07h
        mov     dh,1
res_loop:
        cmp     dl,0
        je      res_loop_done
        dec     dl
        shl     dh,1
        jmp     res_loop
res_loop_done:
        not     dh
        and     bl,dh
        ret

        align 4
deref_ix_ebx    proc    near
        inc     REGPC
        movsx   ebx,byte ptr [REGPC]
        movzx   edx,REGIX
        add     ebx,edx
        add     ebx,dword ptr [_memstart]
        ret
deref_ix_ebx    endp

        align 4
deref_iy_ebx    proc    near
        inc     REGPC
        movsx   ebx,byte ptr [REGPC]
        movzx   edx,REGIY
        add     ebx,edx
        add     ebx,dword ptr [_memstart]
        ret
deref_iy_ebx    endp

_TEXT   ends

_DATA   segment dword public use32 'DATA'
        align   4
_opcodetable:
        dd      doNOP           ; 0x00
        dd      ld_bcnn         ; 0x01
        dd      ld_bcia         ; 0x02
        dd      inc_bc          ; 0x03
        dd      inc_b           ; 0x04
        dd      dec_b           ; 0x05
        dd      ld_b_n          ; 0x06
        dd      rlca            ; 0x07
        dd      ex_af_afp       ; 0x08
        dd      add_hl_bc       ; 0x09
        dd      ld_a_bci        ; 0x0a
        dd      dec_bc          ; 0x0b
        dd      inc_c           ; 0x0c
        dd      dec_c           ; 0x0d
        dd      ld_c_n          ; 0x0e
        dd      rrca            ; 0x0f
        dd      djnz            ; 0x10
        dd      ld_denn         ; 0x11
        dd      ld_dei_a        ; 0x12
        dd      inc_de          ; 0x13
        dd      inc_d           ; 0x14
        dd      dec_d           ; 0x15
        dd      ld_d_n          ; 0x16
        dd      rla             ; 0x17
        dd      jr              ; 0x18
        dd      add_hl_de       ; 0x19
        dd      ld_a_dei        ; 0x1a
        dd      dec_de          ; 0x1b
        dd      inc_e           ; 0x1c
        dd      dec_e           ; 0x1d
        dd      ld_e_n          ; 0x1e
        dd      rra             ; 0x1f
        dd      jr_nz           ; 0x20
        dd      ld_hlnn         ; 0x21
        dd      ld_ext_hl       ; 0x22
        dd      inc_hl          ; 0x23
        dd      inc_h           ; 0x24
        dd      dec_h           ; 0x25
        dd      ld_h_n          ; 0x26
        dd      do_daa          ; 0x27
        dd      jr_z            ; 0x28
        dd      add_hl_hl       ; 0x29
        dd      ld_hl_ext       ; 0x2a
        dd      dec_hl          ; 0x2b
        dd      inc_l           ; 0x2c
        dd      dec_l           ; 0x2d
        dd      ld_l_n          ; 0x2e
        dd      cpl             ; 0x2f
        dd      jr_nc           ; 0x30
        dd      ld_spnn         ; 0x31
        dd      ld_ext_a        ; 0x32
        dd      inc_sp          ; 0x33
        dd      inc_hli         ; 0x34
        dd      dec_hli         ; 0x35
        dd      ld_hli_n        ; 0x36
        dd      scf             ; 0x37
        dd      jr_c            ; 0x38
        dd      add_hl_sp       ; 0x39
        dd      ld_a_ext        ; 0x3a
        dd      dec_sp          ; 0x3b
        dd      inc_a           ; 0x3c
        dd      dec_a           ; 0x3d
        dd      ld_a_n          ; 0x3e
        dd      ccf             ; 0x3f
        dd      doNOP           ; 0x40  ld b,b
        dd      ld_b_c          ; 0x41
        dd      ld_b_d          ; 0x42
        dd      ld_b_e          ; 0x43
        dd      ld_b_h          ; 0x44
        dd      ld_b_l          ; 0x45
        dd      ld_b_hli        ; 0x46
        dd      ld_b_a          ; 0x47
        dd      ld_c_b          ; 0x48
        dd      doNOP           ; 0x49  ld c,c
        dd      ld_c_d          ; 0x4a
        dd      ld_c_e          ; 0x4b
        dd      ld_c_h          ; 0x4c
        dd      ld_c_l          ; 0x4d
        dd      ld_c_hli        ; 0x4e
        dd      ld_c_a          ; 0x4f
        dd      ld_d_b          ; 0x50
        dd      ld_d_c          ; 0x51
        dd      doNOP           ; 0x52  ld d,d
        dd      ld_d_e          ; 0x53
        dd      ld_d_h          ; 0x54
        dd      ld_d_l          ; 0x55
        dd      ld_d_hli        ; 0x56
        dd      ld_d_a          ; 0x57
        dd      ld_e_b          ; 0x58
        dd      ld_e_c          ; 0x59
        dd      ld_e_d          ; 0x5a
        dd      doNOP           ; 0x5b  ld e,e
        dd      ld_e_h          ; 0x5c
        dd      ld_e_l          ; 0x5d
        dd      ld_e_hli        ; 0x5e
        dd      ld_e_a          ; 0x5f
        dd      ld_h_b          ; 0x60
        dd      ld_h_c          ; 0x61
        dd      ld_h_d          ; 0x62
        dd      ld_h_e          ; 0x63
        dd      doNOP           ; 0x64  ld h,h
        dd      ld_h_l          ; 0x65
        dd      ld_h_hli        ; 0x66
        dd      ld_h_a          ; 0x67
        dd      ld_l_b          ; 0x68
        dd      ld_l_c          ; 0x69
        dd      ld_l_d          ; 0x6a
        dd      ld_l_e          ; 0x6b
        dd      ld_l_h          ; 0x6c
        dd      doNOP           ; 0x6d  ld l,l
        dd      ld_l_hli        ; 0x6e
        dd      ld_l_a          ; 0x6f
        dd      ld_hli_b        ; 0x70
        dd      ld_hli_c        ; 0x71
        dd      ld_hli_d        ; 0x72
        dd      ld_hli_e        ; 0x73
        dd      ld_hli_h        ; 0x74
        dd      ld_hli_l        ; 0x75
        dd      do_halt         ; 0x76
        dd      ld_hli_a        ; 0x77
        dd      ld_a_b          ; 0x78
        dd      ld_a_c          ; 0x79
        dd      ld_a_d          ; 0x7a
        dd      ld_a_e          ; 0x7b
        dd      ld_a_h          ; 0x7c
        dd      ld_a_l          ; 0x7d
        dd      ld_a_hli        ; 0x7e
        dd      doNOP           ; 0x7f  ld a,a
        dd      add_a_b         ; 0x80
        dd      add_a_c         ; 0x81
        dd      add_a_d         ; 0x82
        dd      add_a_e         ; 0x83
        dd      add_a_h         ; 0x84
        dd      add_a_l         ; 0x85
        dd      add_a_hli       ; 0x86
        dd      add_a_a         ; 0x87
        dd      adc_a_b         ; 0x88
        dd      adc_a_c         ; 0x89
        dd      adc_a_d         ; 0x8a
        dd      adc_a_e         ; 0x8b
        dd      adc_a_h         ; 0x8c
        dd      adc_a_l         ; 0x8d
        dd      adc_a_hli       ; 0x8e
        dd      adc_a_a         ; 0x8f
        dd      sub_a_b         ; 0x90
        dd      sub_a_c         ; 0x91
        dd      sub_a_d         ; 0x92
        dd      sub_a_e         ; 0x93
        dd      sub_a_h         ; 0x94
        dd      sub_a_l         ; 0x95
        dd      sub_a_hli       ; 0x96
        dd      sub_a_a         ; 0x97
        dd      sbc_a_b         ; 0x98
        dd      sbc_a_c         ; 0x99
        dd      sbc_a_d         ; 0x9a
        dd      sbc_a_e         ; 0x9b
        dd      sbc_a_h         ; 0x9c
        dd      sbc_a_l         ; 0x9d
        dd      sbc_a_hli       ; 0x9e
        dd      sbc_a_a         ; 0x9f
        dd      and_b           ; 0xa0
        dd      and_c           ; 0xa1
        dd      and_d           ; 0xa2
        dd      and_e           ; 0xa3
        dd      and_h           ; 0xa4
        dd      and_l           ; 0xa5
        dd      and_hli         ; 0xa6
        dd      and_a           ; 0xa7
        dd      xor_b           ; 0xa8
        dd      xor_c           ; 0xa9
        dd      xor_d           ; 0xaa
        dd      xor_e           ; 0xab
        dd      xor_h           ; 0xac
        dd      xor_l           ; 0xad
        dd      xor_hli         ; 0xae
        dd      xor_a           ; 0xaf
        dd      or_b            ; 0xb0
        dd      or_c            ; 0xb1
        dd      or_d            ; 0xb2
        dd      or_e            ; 0xb3
        dd      or_h            ; 0xb4
        dd      or_l            ; 0xb5
        dd      or_hli          ; 0xb6
        dd      or_a            ; 0xb7
        dd      cp_b            ; 0xb8
        dd      cp_c            ; 0xb9
        dd      cp_d            ; 0xba
        dd      cp_e            ; 0xbb
        dd      cp_h            ; 0xbc
        dd      cp_l            ; 0xbd
        dd      cp_hli          ; 0xbe
        dd      cp_a            ; 0xbf
        dd      ret_nz          ; 0xc0
        dd      pop_bc          ; 0xc1
        dd      jp_nz           ; 0xc2
        dd      jp_nn           ; 0xc3
        dd      call_nz         ; 0xc4
        dd      push_bc         ; 0xc5
        dd      add_a_n         ; 0xc6
        dd      rst             ; 0xc7
        dd      ret_z           ; 0xc8
        dd      return          ; 0xc9
        dd      jp_z            ; 0xca
        dd      doCB            ; 0xcb
        dd      call_z          ; 0xcc
        dd      call_nn         ; 0xcd
        dd      adc_a_n         ; 0xce
        dd      rst             ; 0xcf
        dd      ret_nc          ; 0xd0
        dd      pop_de          ; 0xd1
        dd      jp_nc           ; 0xd2
        dd      out_n_a         ; 0xd3
        dd      call_nc         ; 0xd4
        dd      push_de         ; 0xd5
        dd      sub_a_n         ; 0xd6
        dd      rst             ; 0xd7
        dd      ret_c           ; 0xd8
        dd      exx             ; 0xd9
        dd      jp_c            ; 0xda
        dd      in_a_n          ; 0xdb
        dd      call_c          ; 0xdc
        dd      doDD            ; 0xdd
        dd      sbc_a_n         ; 0xde
        dd      rst             ; 0xdf
        dd      ret_po          ; 0xe0
        dd      pop_hl          ; 0xe1
        dd      jp_po           ; 0xe2
        dd      ex_spi_hl       ; 0xe3
        dd      call_po         ; 0xe4
        dd      push_hl         ; 0xe5
        dd      and_n           ; 0xe6
        dd      rst             ; 0xe7
        dd      ret_pe          ; 0xe8
        dd      jp_hl           ; 0xe9
        dd      jp_pe           ; 0xea
        dd      ex_de_hl        ; 0xeb
        dd      call_pe         ; 0xec
        dd      doED            ; 0xed
        dd      xor_n           ; 0xee
        dd      rst             ; 0xef
        dd      ret_p           ; 0xf0
        dd      pop_af          ; 0xf1
        dd      jp_p            ; 0xf2
        dd      do_di           ; 0xf3
        dd      call_p          ; 0xf4
        dd      push_af         ; 0xf5
        dd      or_n            ; 0xf6
        dd      rst             ; 0xf7
        dd      ret_m           ; 0xf8
        dd      ld_sp_hl        ; 0xf9
        dd      jp_m            ; 0xfa
        dd      ei              ; 0xfb
        dd      call_m          ; 0xfc
        dd      doFD            ; 0xfd
        dd      cp_n            ; 0xfe
        dd      rst             ; 0xff

_opcodetableDD:
        dd      undefined       ; 0x00
        dd      undefined       ; 0x01
        dd      undefined       ; 0x02
        dd      undefined       ; 0x03
        dd      undefined       ; 0x04
        dd      undefined       ; 0x05
        dd      undefined       ; 0x06
        dd      undefined       ; 0x07
        dd      undefined       ; 0x08
        dd      add_ix_bc       ; 0x09
        dd      undefined       ; 0x0a
        dd      undefined       ; 0x0b
        dd      undefined       ; 0x0c
        dd      undefined       ; 0x0d
        dd      undefined       ; 0x0e
        dd      undefined       ; 0x0f
        dd      undefined       ; 0x10
        dd      undefined       ; 0x11
        dd      undefined       ; 0x12
        dd      undefined       ; 0x13
        dd      undefined       ; 0x14
        dd      undefined       ; 0x15
        dd      undefined       ; 0x16
        dd      undefined       ; 0x17
        dd      undefined       ; 0x18
        dd      add_ix_de       ; 0x19
        dd      undefined       ; 0x1a
        dd      undefined       ; 0x1b
        dd      undefined       ; 0x1c
        dd      undefined       ; 0x1d
        dd      undefined       ; 0x1e
        dd      undefined       ; 0x1f
        dd      undefined       ; 0x20
        dd      ld_ix_nn        ; 0x21
        dd      ld_ext_ix       ; 0x22
        dd      inc_ix          ; 0x23
        dd      undefined       ; 0x24
        dd      undefined       ; 0x25
        dd      undefined       ; 0x26
        dd      undefined       ; 0x27
        dd      undefined       ; 0x28
        dd      add_ix_ix       ; 0x29
        dd      ld_ix_ext       ; 0x2a
        dd      dec_ix          ; 0x2b
        dd      undefined       ; 0x2c
        dd      undefined       ; 0x2d
        dd      undefined       ; 0x2e
        dd      undefined       ; 0x2f
        dd      undefined       ; 0x30
        dd      undefined       ; 0x31
        dd      undefined       ; 0x32
        dd      undefined       ; 0x33
        dd      inc_ixi         ; 0x34
        dd      dec_ixi         ; 0x35
        dd      ld_ixi_n        ; 0x36
        dd      undefined       ; 0x37
        dd      undefined       ; 0x38
        dd      add_ix_sp       ; 0x39
        dd      undefined       ; 0x3a
        dd      undefined       ; 0x3b
        dd      undefined       ; 0x3c
        dd      undefined       ; 0x3d
        dd      undefined       ; 0x3e
        dd      undefined       ; 0x3f
        dd      undefined       ; 0x40
        dd      undefined       ; 0x41
        dd      undefined       ; 0x42
        dd      undefined       ; 0x43
        dd      undefined       ; 0x44
        dd      undefined       ; 0x45
        dd      ld_b_ixi        ; 0x46
        dd      undefined       ; 0x47
        dd      undefined       ; 0x48
        dd      undefined       ; 0x49
        dd      undefined       ; 0x4a
        dd      undefined       ; 0x4b
        dd      undefined       ; 0x4c
        dd      undefined       ; 0x4d
        dd      ld_c_ixi        ; 0x4e
        dd      undefined       ; 0x4f
        dd      undefined       ; 0x50
        dd      undefined       ; 0x51
        dd      undefined       ; 0x52
        dd      undefined       ; 0x53
        dd      undefined       ; 0x54
        dd      undefined       ; 0x55
        dd      ld_d_ixi        ; 0x56
        dd      undefined       ; 0x57
        dd      undefined       ; 0x58
        dd      undefined       ; 0x59
        dd      undefined       ; 0x5a
        dd      undefined       ; 0x5b
        dd      undefined       ; 0x5c
        dd      undefined       ; 0x5d
        dd      ld_e_ixi        ; 0x5e
        dd      undefined       ; 0x5f
        dd      undefined       ; 0x60
        dd      undefined       ; 0x61
        dd      undefined       ; 0x62
        dd      undefined       ; 0x63
        dd      undefined       ; 0x64
        dd      undefined       ; 0x65
        dd      ld_h_ixi        ; 0x66
        dd      undefined       ; 0x67
        dd      undefined       ; 0x68
        dd      undefined       ; 0x69
        dd      undefined       ; 0x6a
        dd      undefined       ; 0x6b
        dd      undefined       ; 0x6c
        dd      undefined       ; 0x6d
        dd      ld_l_ixi        ; 0x6e
        dd      undefined       ; 0x6f
        dd      ld_ixi_b        ; 0x70
        dd      ld_ixi_c        ; 0x71
        dd      ld_ixi_d        ; 0x72
        dd      ld_ixi_e        ; 0x73
        dd      ld_ixi_h        ; 0x74
        dd      ld_ixi_l        ; 0x75
        dd      undefined       ; 0x76
        dd      ld_ixi_a        ; 0x77
        dd      undefined       ; 0x78
        dd      undefined       ; 0x79
        dd      undefined       ; 0x7a
        dd      undefined       ; 0x7b
        dd      undefined       ; 0x7c
        dd      undefined       ; 0x7d
        dd      ld_a_ixi        ; 0x7e
        dd      undefined       ; 0x7f
        dd      undefined       ; 0x80
        dd      undefined       ; 0x81
        dd      undefined       ; 0x82
        dd      undefined       ; 0x83
        dd      undefined       ; 0x84
        dd      undefined       ; 0x85
        dd      add_a_ixi       ; 0x86
        dd      undefined       ; 0x87
        dd      undefined       ; 0x88
        dd      undefined       ; 0x89
        dd      undefined       ; 0x8a
        dd      undefined       ; 0x8b
        dd      undefined       ; 0x8c
        dd      undefined       ; 0x8d
        dd      adc_a_ixi       ; 0x8e
        dd      undefined       ; 0x8f
        dd      undefined       ; 0x90
        dd      undefined       ; 0x91
        dd      undefined       ; 0x92
        dd      undefined       ; 0x93
        dd      undefined       ; 0x94
        dd      undefined       ; 0x95
        dd      sub_a_ixi       ; 0x96
        dd      undefined       ; 0x97
        dd      undefined       ; 0x98
        dd      undefined       ; 0x99
        dd      undefined       ; 0x9a
        dd      undefined       ; 0x9b
        dd      undefined       ; 0x9c
        dd      undefined       ; 0x9d
        dd      sbc_a_ixi       ; 0x9e
        dd      undefined       ; 0x9f
        dd      undefined       ; 0xa0
        dd      undefined       ; 0xa1
        dd      undefined       ; 0xa2
        dd      undefined       ; 0xa3
        dd      undefined       ; 0xa4
        dd      undefined       ; 0xa5
        dd      and_ixi         ; 0xa6
        dd      undefined       ; 0xa7
        dd      undefined       ; 0xa8
        dd      undefined       ; 0xa9
        dd      undefined       ; 0xaa
        dd      undefined       ; 0xab
        dd      undefined       ; 0xac
        dd      undefined       ; 0xad
        dd      xor_ixi         ; 0xae
        dd      undefined       ; 0xaf
        dd      undefined       ; 0xb0
        dd      undefined       ; 0xb1
        dd      undefined       ; 0xb2
        dd      undefined       ; 0xb3
        dd      undefined       ; 0xb4
        dd      undefined       ; 0xb5
        dd      or_ixi          ; 0xb6
        dd      undefined       ; 0xb7
        dd      undefined       ; 0xb8
        dd      undefined       ; 0xb9
        dd      undefined       ; 0xba
        dd      undefined       ; 0xbb
        dd      undefined       ; 0xbc
        dd      undefined       ; 0xbd
        dd      cp_ixi          ; 0xbe
        dd      undefined       ; 0xbf
        dd      undefined       ; 0xc0
        dd      undefined       ; 0xc1
        dd      undefined       ; 0xc2
        dd      undefined       ; 0xc3
        dd      undefined       ; 0xc4
        dd      undefined       ; 0xc5
        dd      undefined       ; 0xc6
        dd      undefined       ; 0xc7
        dd      undefined       ; 0xc8
        dd      undefined       ; 0xc9
        dd      undefined       ; 0xca
        dd      doDDCB          ; 0xcb
        dd      undefined       ; 0xcc
        dd      undefined       ; 0xcd
        dd      undefined       ; 0xce
        dd      undefined       ; 0xcf
        dd      undefined       ; 0xd0
        dd      undefined       ; 0xd1
        dd      undefined       ; 0xd2
        dd      undefined       ; 0xd3
        dd      undefined       ; 0xd4
        dd      undefined       ; 0xd5
        dd      undefined       ; 0xd6
        dd      undefined       ; 0xd7
        dd      undefined       ; 0xd8
        dd      undefined       ; 0xd9
        dd      undefined       ; 0xda
        dd      undefined       ; 0xdb
        dd      undefined       ; 0xdc
        dd      undefined       ; 0xdd
        dd      undefined       ; 0xde
        dd      undefined       ; 0xdf
        dd      undefined       ; 0xe0
        dd      pop_ix          ; 0xe1
        dd      undefined       ; 0xe2
        dd      ex_spi_ix       ; 0xe3
        dd      undefined       ; 0xe4
        dd      push_ix         ; 0xe5
        dd      undefined       ; 0xe6
        dd      undefined       ; 0xe7
        dd      undefined       ; 0xe8
        dd      jp_ix           ; 0xe9
        dd      undefined       ; 0xea
        dd      undefined       ; 0xeb
        dd      undefined       ; 0xec
        dd      undefined       ; 0xed
        dd      undefined       ; 0xee
        dd      undefined       ; 0xef
        dd      undefined       ; 0xf0
        dd      undefined       ; 0xf1
        dd      undefined       ; 0xf2
        dd      undefined       ; 0xf3
        dd      undefined       ; 0xf4
        dd      undefined       ; 0xf5
        dd      undefined       ; 0xf6
        dd      undefined       ; 0xf7
        dd      undefined       ; 0xf8
        dd      ld_sp_ix        ; 0xf9
        dd      undefined       ; 0xfa
        dd      undefined       ; 0xfb
        dd      undefined       ; 0xfc
        dd      undefined       ; 0xfd
        dd      undefined       ; 0xfe
        dd      undefined       ; 0xff

_opcodetableFD:
        dd      undefined       ; 0x00
        dd      undefined       ; 0x01
        dd      undefined       ; 0x02
        dd      undefined       ; 0x03
        dd      undefined       ; 0x04
        dd      undefined       ; 0x05
        dd      undefined       ; 0x06
        dd      undefined       ; 0x07
        dd      undefined       ; 0x08
        dd      add_iy_bc       ; 0x09
        dd      undefined       ; 0x0a
        dd      undefined       ; 0x0b
        dd      undefined       ; 0x0c
        dd      undefined       ; 0x0d
        dd      undefined       ; 0x0e
        dd      undefined       ; 0x0f
        dd      undefined       ; 0x10
        dd      undefined       ; 0x11
        dd      undefined       ; 0x12
        dd      undefined       ; 0x13
        dd      undefined       ; 0x14
        dd      undefined       ; 0x15
        dd      undefined       ; 0x16
        dd      undefined       ; 0x17
        dd      undefined       ; 0x18
        dd      add_iy_de       ; 0x19
        dd      undefined       ; 0x1a
        dd      undefined       ; 0x1b
        dd      undefined       ; 0x1c
        dd      undefined       ; 0x1d
        dd      undefined       ; 0x1e
        dd      undefined       ; 0x1f
        dd      undefined       ; 0x20
        dd      ld_iy_nn        ; 0x21
        dd      ld_ext_iy       ; 0x22
        dd      inc_iy          ; 0x23
        dd      undefined       ; 0x24
        dd      undefined       ; 0x25
        dd      undefined       ; 0x26
        dd      undefined       ; 0x27
        dd      undefined       ; 0x28
        dd      add_iy_iy       ; 0x29
        dd      ld_iy_ext       ; 0x2a
        dd      dec_iy          ; 0x2b
        dd      undefined       ; 0x2c
        dd      undefined       ; 0x2d
        dd      undefined       ; 0x2e
        dd      undefined       ; 0x2f
        dd      undefined       ; 0x30
        dd      undefined       ; 0x31
        dd      undefined       ; 0x32
        dd      undefined       ; 0x33
        dd      inc_iyi         ; 0x34
        dd      dec_iyi         ; 0x35
        dd      ld_iyi_n        ; 0x36
        dd      undefined       ; 0x37
        dd      undefined       ; 0x38
        dd      add_iy_sp       ; 0x39
        dd      undefined       ; 0x3a
        dd      undefined       ; 0x3b
        dd      undefined       ; 0x3c
        dd      undefined       ; 0x3d
        dd      undefined       ; 0x3e
        dd      undefined       ; 0x3f
        dd      undefined       ; 0x40
        dd      undefined       ; 0x41
        dd      undefined       ; 0x42
        dd      undefined       ; 0x43
        dd      undefined       ; 0x44
        dd      undefined       ; 0x45
        dd      ld_b_iyi        ; 0x46
        dd      undefined       ; 0x47
        dd      undefined       ; 0x48
        dd      undefined       ; 0x49
        dd      undefined       ; 0x4a
        dd      undefined       ; 0x4b
        dd      undefined       ; 0x4c
        dd      undefined       ; 0x4d
        dd      ld_c_iyi        ; 0x4e
        dd      undefined       ; 0x4f
        dd      undefined       ; 0x50
        dd      undefined       ; 0x51
        dd      undefined       ; 0x52
        dd      undefined       ; 0x53
        dd      undefined       ; 0x54
        dd      undefined       ; 0x55
        dd      ld_d_iyi        ; 0x56
        dd      undefined       ; 0x57
        dd      undefined       ; 0x58
        dd      undefined       ; 0x59
        dd      undefined       ; 0x5a
        dd      undefined       ; 0x5b
        dd      undefined       ; 0x5c
        dd      undefined       ; 0x5d
        dd      ld_e_iyi        ; 0x5e
        dd      undefined       ; 0x5f
        dd      undefined       ; 0x60
        dd      undefined       ; 0x61
        dd      undefined       ; 0x62
        dd      undefined       ; 0x63
        dd      undefined       ; 0x64
        dd      undefined       ; 0x65
        dd      ld_h_iyi        ; 0x66
        dd      undefined       ; 0x67
        dd      undefined       ; 0x68
        dd      undefined       ; 0x69
        dd      undefined       ; 0x6a
        dd      undefined       ; 0x6b
        dd      undefined       ; 0x6c
        dd      undefined       ; 0x6d
        dd      ld_l_iyi        ; 0x6e
        dd      undefined       ; 0x6f
        dd      ld_iyi_b        ; 0x70
        dd      ld_iyi_c        ; 0x71
        dd      ld_iyi_d        ; 0x72
        dd      ld_iyi_e        ; 0x73
        dd      ld_iyi_h        ; 0x74
        dd      ld_iyi_l        ; 0x75
        dd      undefined       ; 0x76
        dd      ld_iyi_a        ; 0x77
        dd      undefined       ; 0x78
        dd      undefined       ; 0x79
        dd      undefined       ; 0x7a
        dd      undefined       ; 0x7b
        dd      undefined       ; 0x7c
        dd      undefined       ; 0x7d
        dd      ld_a_iyi        ; 0x7e
        dd      undefined       ; 0x7f
        dd      undefined       ; 0x80
        dd      undefined       ; 0x81
        dd      undefined       ; 0x82
        dd      undefined       ; 0x83
        dd      undefined       ; 0x84
        dd      undefined       ; 0x85
        dd      add_a_iyi       ; 0x86
        dd      undefined       ; 0x87
        dd      undefined       ; 0x88
        dd      undefined       ; 0x89
        dd      undefined       ; 0x8a
        dd      undefined       ; 0x8b
        dd      undefined       ; 0x8c
        dd      undefined       ; 0x8d
        dd      adc_a_iyi       ; 0x8e
        dd      undefined       ; 0x8f
        dd      undefined       ; 0x90
        dd      undefined       ; 0x91
        dd      undefined       ; 0x92
        dd      undefined       ; 0x93
        dd      undefined       ; 0x94
        dd      undefined       ; 0x95
        dd      sub_a_iyi       ; 0x96
        dd      undefined       ; 0x97
        dd      undefined       ; 0x98
        dd      undefined       ; 0x99
        dd      undefined       ; 0x9a
        dd      undefined       ; 0x9b
        dd      undefined       ; 0x9c
        dd      undefined       ; 0x9d
        dd      sbc_a_iyi       ; 0x9e
        dd      undefined       ; 0x9f
        dd      undefined       ; 0xa0
        dd      undefined       ; 0xa1
        dd      undefined       ; 0xa2
        dd      undefined       ; 0xa3
        dd      undefined       ; 0xa4
        dd      undefined       ; 0xa5
        dd      and_iyi         ; 0xa6
        dd      undefined       ; 0xa7
        dd      undefined       ; 0xa8
        dd      undefined       ; 0xa9
        dd      undefined       ; 0xaa
        dd      undefined       ; 0xab
        dd      undefined       ; 0xac
        dd      undefined       ; 0xad
        dd      xor_iyi         ; 0xae
        dd      undefined       ; 0xaf
        dd      undefined       ; 0xb0
        dd      undefined       ; 0xb1
        dd      undefined       ; 0xb2
        dd      undefined       ; 0xb3
        dd      undefined       ; 0xb4
        dd      undefined       ; 0xb5
        dd      or_iyi          ; 0xb6
        dd      undefined       ; 0xb7
        dd      undefined       ; 0xb8
        dd      undefined       ; 0xb9
        dd      undefined       ; 0xba
        dd      undefined       ; 0xbb
        dd      undefined       ; 0xbc
        dd      undefined       ; 0xbd
        dd      cp_iyi          ; 0xbe
        dd      undefined       ; 0xbf
        dd      undefined       ; 0xc0
        dd      undefined       ; 0xc1
        dd      undefined       ; 0xc2
        dd      undefined       ; 0xc3
        dd      undefined       ; 0xc4
        dd      undefined       ; 0xc5
        dd      undefined       ; 0xc6
        dd      undefined       ; 0xc7
        dd      undefined       ; 0xc8
        dd      undefined       ; 0xc9
        dd      undefined       ; 0xca
        dd      doFDCB          ; 0xcb
        dd      undefined       ; 0xcc
        dd      undefined       ; 0xcd
        dd      undefined       ; 0xce
        dd      undefined       ; 0xcf
        dd      undefined       ; 0xd0
        dd      undefined       ; 0xd1
        dd      undefined       ; 0xd2
        dd      undefined       ; 0xd3
        dd      undefined       ; 0xd4
        dd      undefined       ; 0xd5
        dd      undefined       ; 0xd6
        dd      undefined       ; 0xd7
        dd      undefined       ; 0xd8
        dd      undefined       ; 0xd9
        dd      undefined       ; 0xda
        dd      undefined       ; 0xdb
        dd      undefined       ; 0xdc
        dd      undefined       ; 0xdd
        dd      undefined       ; 0xde
        dd      undefined       ; 0xdf
        dd      undefined       ; 0xe0
        dd      pop_iy          ; 0xe1
        dd      undefined       ; 0xe2
        dd      ex_spi_iy       ; 0xe3
        dd      undefined       ; 0xe4
        dd      push_iy         ; 0xe5
        dd      undefined       ; 0xe6
        dd      undefined       ; 0xe7
        dd      undefined       ; 0xe8
        dd      jp_iy           ; 0xe9
        dd      undefined       ; 0xea
        dd      undefined       ; 0xeb
        dd      undefined       ; 0xec
        dd      undefined       ; 0xed
        dd      undefined       ; 0xee
        dd      undefined       ; 0xef
        dd      undefined       ; 0xf0
        dd      undefined       ; 0xf1
        dd      undefined       ; 0xf2
        dd      undefined       ; 0xf3
        dd      undefined       ; 0xf4
        dd      undefined       ; 0xf5
        dd      undefined       ; 0xf6
        dd      undefined       ; 0xf7
        dd      undefined       ; 0xf8
        dd      ld_sp_iy        ; 0xf9
        dd      undefined       ; 0xfa
        dd      undefined       ; 0xfb
        dd      undefined       ; 0xfc
        dd      undefined       ; 0xfd
        dd      undefined       ; 0xfe
        dd      undefined       ; 0xff

_opcodetableED:
        dd      undefined       ; 0x00
        dd      undefined       ; 0x01
        dd      undefined       ; 0x02
        dd      undefined       ; 0x03
        dd      undefined       ; 0x04
        dd      undefined       ; 0x05
        dd      undefined       ; 0x06
        dd      undefined       ; 0x07
        dd      undefined       ; 0x08
        dd      undefined       ; 0x09
        dd      undefined       ; 0x0a
        dd      undefined       ; 0x0b
        dd      undefined       ; 0x0c
        dd      undefined       ; 0x0d
        dd      undefined       ; 0x0e
        dd      undefined       ; 0x0f
        dd      undefined       ; 0x10
        dd      undefined       ; 0x11
        dd      undefined       ; 0x12
        dd      undefined       ; 0x13
        dd      undefined       ; 0x14
        dd      undefined       ; 0x15
        dd      undefined       ; 0x16
        dd      undefined       ; 0x17
        dd      undefined       ; 0x18
        dd      undefined       ; 0x19
        dd      undefined       ; 0x1a
        dd      undefined       ; 0x1b
        dd      undefined       ; 0x1c
        dd      undefined       ; 0x1d
        dd      undefined       ; 0x1e
        dd      undefined       ; 0x1f
        dd      undefined       ; 0x20
        dd      undefined       ; 0x21
        dd      undefined       ; 0x22
        dd      undefined       ; 0x23
        dd      undefined       ; 0x24
        dd      undefined       ; 0x25
        dd      undefined       ; 0x26
        dd      undefined       ; 0x27
        dd      undefined       ; 0x28
        dd      undefined       ; 0x29
        dd      undefined       ; 0x2a
        dd      undefined       ; 0x2b
        dd      undefined       ; 0x2c
        dd      undefined       ; 0x2d
        dd      undefined       ; 0x2e
        dd      undefined       ; 0x2f
        dd      undefined       ; 0x30
        dd      undefined       ; 0x31
        dd      undefined       ; 0x32
        dd      undefined       ; 0x33
        dd      undefined       ; 0x34
        dd      undefined       ; 0x35
        dd      undefined       ; 0x36
        dd      undefined       ; 0x37
        dd      undefined       ; 0x38
        dd      undefined       ; 0x39
        dd      undefined       ; 0x3a
        dd      undefined       ; 0x3b
        dd      undefined       ; 0x3c
        dd      undefined       ; 0x3d
        dd      undefined       ; 0x3e
        dd      undefined       ; 0x3f
        dd      in_r_c          ; 0x40
        dd      out_c_b         ; 0x41
        dd      sbc_hl_bc       ; 0x42
        dd      ld_ext_bc       ; 0x43
        dd      do_neg          ; 0x44
        dd      do_retn         ; 0x45
        dd      im0             ; 0x46
        dd      ld_i_a          ; 0x47
        dd      in_r_c          ; 0x48
        dd      out_c_c         ; 0x49
        dd      adc_hl_bc       ; 0x4a
        dd      ld_bc_ext       ; 0x4b
        dd      do_neg          ; 0x4c  undocumented
        dd      do_reti         ; 0x4d
        dd      im0             ; 0x4e  undocumented
        dd      ld_r_a          ; 0x4f
        dd      in_r_c          ; 0x50
        dd      out_c_d         ; 0x51
        dd      sbc_hl_de       ; 0x52
        dd      ld_ext_de       ; 0x53
        dd      do_neg          ; 0x54  undocumented
        dd      do_retn         ; 0x55  undocumented
        dd      im1             ; 0x56
        dd      ld_a_i          ; 0x57
        dd      in_r_c          ; 0x58
        dd      out_c_e         ; 0x59
        dd      adc_hl_de       ; 0x5a
        dd      ld_de_ext       ; 0x5b
        dd      do_neg          ; 0x5c  undocumented
        dd      do_retn         ; 0x5d  undocumented
        dd      im2             ; 0x5e
        dd      ld_a_r          ; 0x5f
        dd      in_r_c          ; 0x60
        dd      out_c_h         ; 0x61
        dd      sbc_hl_hl       ; 0x62
        dd      ld_ext_hl2      ; 0x63
        dd      do_neg          ; 0x64  undocumented
        dd      do_retn         ; 0x65  undocumented
        dd      im0             ; 0x66  undocumented
        dd      rrd             ; 0x67
        dd      in_r_c          ; 0x68
        dd      out_c_l         ; 0x69
        dd      adc_hl_hl       ; 0x6a
        dd      ld_hl_ext2      ; 0x6b
        dd      do_neg          ; 0x6c  undocumented
        dd      do_retn         ; 0x6d  undocumented
        dd      im0             ; 0x6e  undocumented
        dd      rld             ; 0x6f
        dd      in_r_c          ; 0x70
        dd      out_c_0         ; 0x71
        dd      sbc_hl_sp       ; 0x72
        dd      ld_ext_sp       ; 0x73
        dd      do_neg          ; 0x74  undocumented
        dd      do_retn         ; 0x75  undocumented
        dd      im1             ; 0x76  undocumented
        dd      undefined       ; 0x77
        dd      in_r_c          ; 0x78
        dd      out_c_a         ; 0x79
        dd      adc_hl_sp       ; 0x7a
        dd      ld_sp_ext       ; 0x7b
        dd      do_neg          ; 0x7c  undocumented
        dd      do_retn         ; 0x7d  undocumented
        dd      im2             ; 0x7e  undocumented
        dd      undefined       ; 0x7f
        dd      undefined       ; 0x80
        dd      undefined       ; 0x81
        dd      undefined       ; 0x82
        dd      undefined       ; 0x83
        dd      undefined       ; 0x84
        dd      undefined       ; 0x85
        dd      undefined       ; 0x86
        dd      undefined       ; 0x87
        dd      undefined       ; 0x88
        dd      undefined       ; 0x89
        dd      undefined       ; 0x8a
        dd      undefined       ; 0x8b
        dd      undefined       ; 0x8c
        dd      undefined       ; 0x8d
        dd      undefined       ; 0x8e
        dd      undefined       ; 0x8f
        dd      undefined       ; 0x90
        dd      undefined       ; 0x91
        dd      undefined       ; 0x92
        dd      undefined       ; 0x93
        dd      undefined       ; 0x94
        dd      undefined       ; 0x95
        dd      undefined       ; 0x96
        dd      undefined       ; 0x97
        dd      undefined       ; 0x98
        dd      undefined       ; 0x99
        dd      undefined       ; 0x9a
        dd      undefined       ; 0x9b
        dd      undefined       ; 0x9c
        dd      undefined       ; 0x9d
        dd      undefined       ; 0x9e
        dd      undefined       ; 0x9f
        dd      ldi             ; 0xa0
        dd      cpi             ; 0xa1
        dd      ini             ; 0xa2
        dd      outi            ; 0xa3
        dd      undefined       ; 0xa4
        dd      undefined       ; 0xa5
        dd      undefined       ; 0xa6
        dd      undefined       ; 0xa7
        dd      ldd             ; 0xa8
        dd      cpd             ; 0xa9
        dd      ind             ; 0xaa
        dd      outd            ; 0xab
        dd      undefined       ; 0xac
        dd      undefined       ; 0xad
        dd      undefined       ; 0xae
        dd      undefined       ; 0xaf
        dd      ldir            ; 0xb0
        dd      cpir            ; 0xb1
        dd      inir            ; 0xb2
        dd      otir            ; 0xb3
        dd      undefined       ; 0xb4
        dd      undefined       ; 0xb5
        dd      undefined       ; 0xb6
        dd      undefined       ; 0xb7
        dd      lddr            ; 0xb8
        dd      cpdr            ; 0xb9
        dd      indr            ; 0xba
        dd      otdr            ; 0xbb
        dd      undefined       ; 0xbc
        dd      undefined       ; 0xbd
        dd      undefined       ; 0xbe
        dd      undefined       ; 0xbf
        dd      undefined       ; 0xc0
        dd      undefined       ; 0xc1
        dd      undefined       ; 0xc2
        dd      undefined       ; 0xc3
        dd      undefined       ; 0xc4
        dd      undefined       ; 0xc5
        dd      undefined       ; 0xc6
        dd      undefined       ; 0xc7
        dd      undefined       ; 0xc8
        dd      undefined       ; 0xc9
        dd      undefined       ; 0xca
        dd      undefined       ; 0xcb
        dd      undefined       ; 0xcc
        dd      undefined       ; 0xcd
        dd      undefined       ; 0xce
        dd      undefined       ; 0xcf
        dd      undefined       ; 0xd0
        dd      undefined       ; 0xd1
        dd      undefined       ; 0xd2
        dd      undefined       ; 0xd3
        dd      undefined       ; 0xd4
        dd      undefined       ; 0xd5
        dd      undefined       ; 0xd6
        dd      undefined       ; 0xd7
        dd      undefined       ; 0xd8
        dd      undefined       ; 0xd9
        dd      undefined       ; 0xda
        dd      undefined       ; 0xdb
        dd      undefined       ; 0xdc
        dd      undefined       ; 0xdd
        dd      undefined       ; 0xde
        dd      undefined       ; 0xdf
        dd      undefined       ; 0xe0
        dd      undefined       ; 0xe1
        dd      undefined       ; 0xe2
        dd      undefined       ; 0xe3
        dd      undefined       ; 0xe4
        dd      undefined       ; 0xe5
        dd      undefined       ; 0xe6
        dd      undefined       ; 0xe7
        dd      undefined       ; 0xe8
        dd      undefined       ; 0xe9
        dd      undefined       ; 0xea
        dd      undefined       ; 0xeb
        dd      undefined       ; 0xec
        dd      undefined       ; 0xed
        dd      undefined       ; 0xee
        dd      undefined       ; 0xef
        dd      undefined       ; 0xf0
        dd      undefined       ; 0xf1
        dd      undefined       ; 0xf2
        dd      undefined       ; 0xf3
        dd      undefined       ; 0xf4
        dd      undefined       ; 0xf5
        dd      undefined       ; 0xf6
        dd      undefined       ; 0xf7
        dd      undefined       ; 0xf8
        dd      undefined       ; 0xf9
        dd      undefined       ; 0xfa
        dd      undefined       ; 0xfb
        dd      undefined       ; 0xfc
        dd      undefined       ; 0xfd
        dd      undefined       ; 0xfe
        dd      undefined       ; 0xff

_opcodetableCB:
        dd      rlc_b           ; 0x00
        dd      rlc_c           ; 0x01
        dd      rlc_d           ; 0x02
        dd      rlc_e           ; 0x03
        dd      rlc_h           ; 0x04
        dd      rlc_l           ; 0x05
        dd      rlc_ind         ; 0x06
        dd      rlc_a           ; 0x07
        dd      rrc_b           ; 0x08
        dd      rrc_c           ; 0x09
        dd      rrc_d           ; 0x0a
        dd      rrc_e           ; 0x0b
        dd      rrc_h           ; 0x0c
        dd      rrc_l           ; 0x0d
        dd      rrc_ind         ; 0x0e
        dd      rrc_a           ; 0x0f
        dd      rl_b            ; 0x10
        dd      rl_c            ; 0x11
        dd      rl_d            ; 0x12
        dd      rl_e            ; 0x13
        dd      rl_h            ; 0x14
        dd      rl_l            ; 0x15
        dd      rl_ind          ; 0x16
        dd      rl_a            ; 0x17
        dd      rr_b            ; 0x18
        dd      rr_c            ; 0x19
        dd      rr_d            ; 0x1a
        dd      rr_e            ; 0x1b
        dd      rr_h            ; 0x1c
        dd      rr_l            ; 0x1d
        dd      rr_ind          ; 0x1e
        dd      rr_a            ; 0x1f
        dd      sla_b           ; 0x20
        dd      sla_c           ; 0x21
        dd      sla_d           ; 0x22
        dd      sla_e           ; 0x23
        dd      sla_h           ; 0x24
        dd      sla_l           ; 0x25
        dd      sla_ind         ; 0x26
        dd      sla_a           ; 0x27
        dd      sra_b           ; 0x28
        dd      sra_c           ; 0x29
        dd      sra_d           ; 0x2a
        dd      sra_e           ; 0x2b
        dd      sra_h           ; 0x2c
        dd      sra_l           ; 0x2d
        dd      sra_ind         ; 0x2e
        dd      sra_a           ; 0x2f
        dd      sll_b           ; 0x30
        dd      sll_c           ; 0x31
        dd      sll_d           ; 0x32
        dd      sll_e           ; 0x33
        dd      sll_h           ; 0x34
        dd      sll_l           ; 0x35
        dd      sll_ind         ; 0x36
        dd      sll_a           ; 0x37
        dd      srl_b           ; 0x38
        dd      srl_c           ; 0x39
        dd      srl_d           ; 0x3a
        dd      srl_e           ; 0x3b
        dd      srl_h           ; 0x3c
        dd      srl_l           ; 0x3d
        dd      srl_ind         ; 0x3e
        dd      srl_a           ; 0x3f
        dd      bit_b           ; 0x40
        dd      bit_c           ; 0x41
        dd      bit_d           ; 0x42
        dd      bit_e           ; 0x43
        dd      bit_h           ; 0x44
        dd      bit_l           ; 0x45
        dd      bit_ind         ; 0x46
        dd      bit_a           ; 0x47
        dd      bit_b           ; 0x48
        dd      bit_c           ; 0x49
        dd      bit_d           ; 0x4a
        dd      bit_e           ; 0x4b
        dd      bit_h           ; 0x4c
        dd      bit_l           ; 0x4d
        dd      bit_ind         ; 0x4e
        dd      bit_a           ; 0x4f
        dd      bit_b           ; 0x50
        dd      bit_c           ; 0x51
        dd      bit_d           ; 0x52
        dd      bit_e           ; 0x53
        dd      bit_h           ; 0x54
        dd      bit_l           ; 0x55
        dd      bit_ind         ; 0x56
        dd      bit_a           ; 0x57
        dd      bit_b           ; 0x58
        dd      bit_c           ; 0x59
        dd      bit_d           ; 0x5a
        dd      bit_e           ; 0x5b
        dd      bit_h           ; 0x5c
        dd      bit_l           ; 0x5d
        dd      bit_ind         ; 0x5e
        dd      bit_a           ; 0x5f
        dd      bit_b           ; 0x60
        dd      bit_c           ; 0x61
        dd      bit_d           ; 0x62
        dd      bit_e           ; 0x63
        dd      bit_h           ; 0x64
        dd      bit_l           ; 0x65
        dd      bit_ind         ; 0x66
        dd      bit_a           ; 0x67
        dd      bit_b           ; 0x68
        dd      bit_c           ; 0x69
        dd      bit_d           ; 0x6a
        dd      bit_e           ; 0x6b
        dd      bit_h           ; 0x6c
        dd      bit_l           ; 0x6d
        dd      bit_ind         ; 0x6e
        dd      bit_a           ; 0x6f
        dd      bit_b           ; 0x70
        dd      bit_c           ; 0x71
        dd      bit_d           ; 0x72
        dd      bit_e           ; 0x73
        dd      bit_h           ; 0x74
        dd      bit_l           ; 0x75
        dd      bit_ind         ; 0x76
        dd      bit_a           ; 0x77
        dd      bit_b           ; 0x78
        dd      bit_c           ; 0x79
        dd      bit_d           ; 0x7a
        dd      bit_e           ; 0x7b
        dd      bit_h           ; 0x7c
        dd      bit_l           ; 0x7d
        dd      bit_ind         ; 0x7e
        dd      bit_a           ; 0x7f
        dd      res_b           ; 0x80
        dd      res_c           ; 0x81
        dd      res_d           ; 0x82
        dd      res_e           ; 0x83
        dd      res_h           ; 0x84
        dd      res_l           ; 0x85
        dd      res_ind         ; 0x86
        dd      res_a           ; 0x87
        dd      res_b           ; 0x88
        dd      res_c           ; 0x89
        dd      res_d           ; 0x8a
        dd      res_e           ; 0x8b
        dd      res_h           ; 0x8c
        dd      res_l           ; 0x8d
        dd      res_ind         ; 0x8e
        dd      res_a           ; 0x8f
        dd      res_b           ; 0x90
        dd      res_c           ; 0x91
        dd      res_d           ; 0x92
        dd      res_e           ; 0x93
        dd      res_h           ; 0x94
        dd      res_l           ; 0x95
        dd      res_ind         ; 0x96
        dd      res_a           ; 0x97
        dd      res_b           ; 0x98
        dd      res_c           ; 0x99
        dd      res_d           ; 0x9a
        dd      res_e           ; 0x9b
        dd      res_h           ; 0x9c
        dd      res_l           ; 0x9d
        dd      res_ind         ; 0x9e
        dd      res_a           ; 0x9f
        dd      res_b           ; 0xa0
        dd      res_c           ; 0xa1
        dd      res_d           ; 0xa2
        dd      res_e           ; 0xa3
        dd      res_h           ; 0xa4
        dd      res_l           ; 0xa5
        dd      res_ind         ; 0xa6
        dd      res_a           ; 0xa7
        dd      res_b           ; 0xa8
        dd      res_c           ; 0xa9
        dd      res_d           ; 0xaa
        dd      res_e           ; 0xab
        dd      res_h           ; 0xac
        dd      res_l           ; 0xad
        dd      res_ind         ; 0xae
        dd      res_a           ; 0xaf
        dd      res_b           ; 0xb0
        dd      res_c           ; 0xb1
        dd      res_d           ; 0xb2
        dd      res_e           ; 0xb3
        dd      res_h           ; 0xb4
        dd      res_l           ; 0xb5
        dd      res_ind         ; 0xb6
        dd      res_a           ; 0xb7
        dd      res_b           ; 0xb8
        dd      res_c           ; 0xb9
        dd      res_d           ; 0xba
        dd      res_e           ; 0xbb
        dd      res_h           ; 0xbc
        dd      res_l           ; 0xbd
        dd      res_ind         ; 0xbe
        dd      res_a           ; 0xbf
        dd      set_b           ; 0xc0
        dd      set_c           ; 0xc1
        dd      set_d           ; 0xc2
        dd      set_e           ; 0xc3
        dd      set_h           ; 0xc4
        dd      set_l           ; 0xc5
        dd      set_ind         ; 0xc6
        dd      set_a           ; 0xc7
        dd      set_b           ; 0xc8
        dd      set_c           ; 0xc9
        dd      set_d           ; 0xca
        dd      set_e           ; 0xcb
        dd      set_h           ; 0xcc
        dd      set_l           ; 0xcd
        dd      set_ind         ; 0xce
        dd      set_a           ; 0xcf
        dd      set_b           ; 0xd0
        dd      set_c           ; 0xd1
        dd      set_d           ; 0xd2
        dd      set_e           ; 0xd3
        dd      set_h           ; 0xd4
        dd      set_l           ; 0xd5
        dd      set_ind         ; 0xd6
        dd      set_a           ; 0xd7
        dd      set_b           ; 0xd8
        dd      set_c           ; 0xd9
        dd      set_d           ; 0xda
        dd      set_e           ; 0xdb
        dd      set_h           ; 0xdc
        dd      set_l           ; 0xdd
        dd      set_ind         ; 0xde
        dd      set_a           ; 0xdf
        dd      set_b           ; 0xe0
        dd      set_c           ; 0xe1
        dd      set_d           ; 0xe2
        dd      set_e           ; 0xe3
        dd      set_h           ; 0xe4
        dd      set_l           ; 0xe5
        dd      set_ind         ; 0xe6
        dd      set_a           ; 0xe7
        dd      set_b           ; 0xe8
        dd      set_c           ; 0xe9
        dd      set_d           ; 0xea
        dd      set_e           ; 0xeb
        dd      set_h           ; 0xec
        dd      set_l           ; 0xed
        dd      set_ind         ; 0xee
        dd      set_a           ; 0xef
        dd      set_b           ; 0xf0
        dd      set_c           ; 0xf1
        dd      set_d           ; 0xf2
        dd      set_e           ; 0xf3
        dd      set_h           ; 0xf4
        dd      set_l           ; 0xf5
        dd      set_ind         ; 0xf6
        dd      set_a           ; 0xf7
        dd      set_b           ; 0xf8
        dd      set_c           ; 0xf9
        dd      set_d           ; 0xfa
        dd      set_e           ; 0xfb
        dd      set_h           ; 0xfc
        dd      set_l           ; 0xfd
        dd      set_ind         ; 0xfe
        dd      set_a           ; 0xff
_DATA   ends

        end

