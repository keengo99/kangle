package suite

import (
	"fmt"
)

type Case struct {
	Name string
	Desc string
	Test func()
}
type Interface interface {
	Init() error
	Test(case_name string)
	Clean()
	List()
	GetName() string
}
type Suite struct {
	Name  string
	Cases []*Case
}

var suites map[string]Interface

func init() {
	suites = make(map[string]Interface, 0)
}
func Register(suite Interface) {
	suites[suite.GetName()] = suite
}

func (this *Suite) AddCase(name string, desc string, test func()) {
	c := &Case{Name: name, Desc: desc, Test: test}
	this.Cases = append(this.Cases, c)
}
func (this *Suite) Test(case_name string) {
	fmt.Printf("test suite [%s] ...\n", this.Name)
	for _, c := range this.Cases {
		if len(case_name) > 0 && c.Name != case_name {
			continue
		}
		fmt.Printf("\tcase\t%16s\t%s\t", c.Name, c.Desc)
		c.Test()
		fmt.Printf("ok\n")
	}
}
func (this *Suite) GetName() string {
	return this.Name
}
func (this *Suite) List() {
	fmt.Printf("----[%s] case list---\n", this.Name)
	for _, c := range this.Cases {
		fmt.Printf("\t[%s]\t[%s]\n", c.Name, c.Desc)
	}
	fmt.Printf("----[%s] total [%d] cases---\n", this.Name, len(this.Cases))
}
