; This is an example program written in assembly which the
; interpreter written in C can process.
;
; Later this will require a _start function label to be defined
; when we start using sections in files and have symbol information.
label:
	loadi r0, #100  ; load 100 into register r0
	add r1, r0      ; add r1 to r0
	div r0, r1      ; divide r0 to r1
	dmp             ; dump registers
	callf $label    ; recursive call
	halt            ; terminate program
