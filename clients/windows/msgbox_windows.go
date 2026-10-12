package main

import (
	"syscall"
	"unsafe"
)

var procMessageBoxW = syscall.NewLazyDLL("user32.dll").NewProc("MessageBoxW")

func syscallUTF16(s string) (unsafe.Pointer, error) {
	p, err := syscall.UTF16PtrFromString(s)
	return unsafe.Pointer(p), err
}
