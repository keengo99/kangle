package kangle

import (
	"fmt"
	"os"
	"test_server/config"
)

func create_license() {
	license_file, err := os.Create(config.Cfg.BasePath + "/license.txt")
	if err != nil {
		panic(err)
	}
	defer license_file.Close()
	n, err := license_file.WriteString(`2
H4sIAAAAAAAAA5Pv5mAAA2bGToay//Jw
Lu+hg1yHDHgYLlT/c/WctS2TX26bYUCA
i0Cg/uKld2eEFVro1+39dG/5mcfKZyQl
jr7ouzFxY1Rn++8fMg1TdD72MzSK5pRd
/nKolSd68rmrX8tq2mtru6Y9uV3Ubhpw
7mQQl6374rUWvidZymr57rl/YVt1ad3l
h0FJwWvritQWx9cA7W8UPXClE0gDAJhz
PDygAAAA
`)
	if err != nil {
		panic(err)
	}
	fmt.Printf("success create license with [%d]\n", n)
}
