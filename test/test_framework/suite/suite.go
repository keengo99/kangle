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
	Name     string
	Cases    []*Case
	CasesMap map[string]*Case
}

var suites map[string]Interface

func init() {
	suites = make(map[string]Interface, 0)
}
func Register(suite Interface) {
	suites[suite.GetName()] = suite
}

func (suite *Suite) AddCase(name string, desc string, test func()) {
	c := &Case{Name: name, Desc: desc, Test: test}
	suite.Cases = append(suite.Cases, c)
	suite.CasesMap[name] = c
}
func (suite *Suite) Test(case_name string) {
	fmt.Printf("test suite [%s] ...\n", suite.Name)
	if len(case_name) > 0 {
		c, ok := suite.CasesMap[case_name]
		if !ok {
			fmt.Printf("cann't found case [%s.%s]\n", suite.Name, case_name)
			suite.List()
			panic("")
		}
		fmt.Printf("\tcase\t%16s\t%s\t", c.Name, c.Desc)
		c.Test()
		fmt.Printf("ok\n")
		return
	}
	for _, c := range suite.Cases {
		fmt.Printf("\tcase\t%16s\t%s\t", c.Name, c.Desc)
		c.Test()
		fmt.Printf("ok\n")
	}
}
func (suite *Suite) GetName() string {
	return suite.Name
}
func (suite *Suite) List() {
	fmt.Printf("----[%s] case list---\n", suite.Name)
	for _, c := range suite.Cases {
		fmt.Printf("\t[%s]\t[%s]\n", c.Name, c.Desc)
	}
	fmt.Printf("----[%s] total [%d] cases---\n", suite.Name, len(suite.Cases))
}
