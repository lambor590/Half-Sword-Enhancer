.data
EXTERN originalFuncs:QWORD
EXTERN proxyState:DWORD

.code
EXTERN WaitForOriginalFunction:PROC

curIndex = 0

PROXY MACRO funcName, waitName, notReadyResult
    funcName PROC
        cmp DWORD PTR [proxyState], 1
        jne waitName
        mov rax, QWORD PTR [originalFuncs + curIndex * 8]
        jmp rax
    funcName ENDP

    waitName PROC FRAME
        sub rsp, 88h
        .allocstack 88h
        .endprolog
        mov QWORD PTR [rsp + 20h], rcx
        mov QWORD PTR [rsp + 28h], rdx
        mov QWORD PTR [rsp + 30h], r8
        mov QWORD PTR [rsp + 38h], r9
        movdqu XMMWORD PTR [rsp + 40h], xmm0
        movdqu XMMWORD PTR [rsp + 50h], xmm1
        movdqu XMMWORD PTR [rsp + 60h], xmm2
        movdqu XMMWORD PTR [rsp + 70h], xmm3
        mov ecx, curIndex
        call WaitForOriginalFunction
        test rax, rax
        jz notReady
        mov rcx, QWORD PTR [rsp + 20h]
        mov rdx, QWORD PTR [rsp + 28h]
        mov r8, QWORD PTR [rsp + 30h]
        mov r9, QWORD PTR [rsp + 38h]
        movdqu xmm0, XMMWORD PTR [rsp + 40h]
        movdqu xmm1, XMMWORD PTR [rsp + 50h]
        movdqu xmm2, XMMWORD PTR [rsp + 60h]
        movdqu xmm3, XMMWORD PTR [rsp + 70h]
        add rsp, 88h
        jmp QWORD PTR [originalFuncs + curIndex * 8]
    notReady:
        mov eax, notReadyResult
        add rsp, 88h
        ret
    waitName ENDP

    curIndex = curIndex + 1
ENDM

include winmm_exports.generated.inc

.ERRNZ curIndex - WINMM_EXPORT_COUNT

END
