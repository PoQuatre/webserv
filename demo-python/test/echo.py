#!/usr/bin/python3
import cgi

print("Content-type: text/html\n\n")

print("<html><body style='text-align:center;'>")
print("<h1 style='color: green;'>webserv</h1>")
form = cgi.FieldStorage()

if form.getvalue("message"):
    # If present, retrieve the value and display a personalized greeting
    name = form.getvalue("message")
    print("<h2>Hello!</h2>")
    print("<a>" + name + "<a/>")
# Close the HTML document
print("</body></html>")
