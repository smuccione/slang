using System;
using System.Reflection;
using System.Windows.Forms;

namespace MyNamespace
{
	public class SimpleClass
	{
        public string MsgBox( string str ) 
		{
                  MessageBox.Show(str, "And now for this message");
                  return ( "this is a test" );
		}
        public string MsgBox2( string str ) 
		{
                  MessageBox.Show(str, "And now for this message");
                  return ( "this is a test" );
		}
	}
}
