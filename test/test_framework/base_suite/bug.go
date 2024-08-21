package base_suite

func check_bug() {
	//multi miss change
	/*
		check_ranges([]RequestRange{
			{262044, 102400, nil, func(resp *http.Response, err error) {
				fmt.Printf("pass1\n")
				common.Assert("x-big-object-miss1", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
			}},
			{100000, 8190, nil, func(resp *http.Response, err error) {
				fmt.Printf("pass2\n")
				common.Assert("x-big-object-miss2", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
			}},
			{0, -1, func(from, to, c int, r *http.Request, w http.ResponseWriter) bool {
				fmt.Printf("pass3\n")
				fmt.Printf("from=[%d],to=[%d] c=[%d]\n", from, to, c)
				if c == 0 {
					common.CreateRange(1024)
					common.SkipCheckRespComplete = true
				}
				return true
			}, func(resp *http.Response, err error) {
				fmt.Printf("pass4\n")
				common.Assert("x-big-object-hit-part4", strings.Contains(resp.Header.Get("X-Cache"), "HIT-PART"))
			}},
		}, false)
	*/
}
