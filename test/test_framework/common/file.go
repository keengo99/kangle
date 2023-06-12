package common

import (
	"io"
	"os"
	"runtime"
)

func ExeExtendFile() string {
	if runtime.GOOS == "windows" {
		return ".exe"
	}
	return ""
}
func DllExtendFile() string {
	if runtime.GOOS == "windows" {
		return ".dll"
	}
	return ".so"
}
func CopyFile(src, dst string) {
	in, err := os.Open(src)
	if err != nil {
		panic(err)
	}
	defer in.Close()
	out, err := os.Create(dst)
	if err != nil {
		panic(err)
	}
	defer func() {
		cerr := out.Close()
		if err == nil {
			err = cerr
		}
		err = os.Chmod(dst, 0755)
	}()
	if _, err = io.Copy(out, in); err != nil {
		panic(err)
	}
	err = out.Sync()
	if err != nil {
		panic(err)
	}
}
