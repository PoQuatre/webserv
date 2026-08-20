#!/usr/bin/env python
import cgi

print("Content-type: text/html\n\n")

print("<html><body style='text-align:center;'>")
print("<h1 style='color: green;'>webserv</h1>")
form = cgi.FieldStorage()

if form.getvalue("name"):
    # If present, retrieve the value and display a personalized greeting
    name = form.getvalue("name")
    print("<h2>Hello, " + name + "!</h2>")


# Close the HTML document
print("</body></html>")
