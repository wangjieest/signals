// test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../include/signals.h"
#include <iostream>
#include <stdio.h>
class CCC
{
public:
	void fn(const char* var)
	{
		printf("%s CCC::fn\n", var);
	}
	void fn1(const char* var)
	{
		printf("%s CCC::fn1\n", var);
	}
private:
	int bb;
};

int main(int argc, char * argv[])
{
	base::signal_t<const char*> sig;
	CCC a;
	{
		base::scoped_connection_t conn = sig.connect(&CCC::fn1, &a);
		auto conn2 = sig.connect(&CCC::fn, &a);
		sig.fire("step 1");
		conn2.disconnect();
		sig.fire("step 2");
		sig.connect(&CCC::fn, &a);
		sig.fire("step 3");
		sig.disconnect(&CCC::fn, &a);
		sig.fire("step 4");
	}
	sig.fire("step 5");

	return 0;

}