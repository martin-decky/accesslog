/*
 * Copyright (c) 2017 Martin Decky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>

using namespace std;
using namespace boost;

/** Parts of domain name */
typedef vector< string> domain_vector;

/** Character list separator */
typedef char_separator< char> separator_type;

/** Character list tokenizer */
typedef tokenizer< separator_type> tokenizer_type;

typedef struct {
	long int year;
	long int month;
	long int day;
	
	long int hour;
	long int minute;
	long int second;
	
	long int offset;
} datetime; /**< Date & time entry */

/** Basic prefix of the domain directories */
static const string prefix = "/home/httpd";

/** Basic suffix for the domain directories */
static string suffix = "";

/** Decode integer from string (base 10)
 *
 * Throws invalid_argument on invalid
 * numerical string.
 *
 * @param decimal String to decode.
 *
 * @return Decoded integer.
 *
 */
static long int decDecode(const string &decimal)
{
	char *err;
	
	long int val = strtol(decimal.c_str(), &err, 10);
	if ((err == NULL) || (*err != (char) 0))
		throw invalid_argument("Not an integer");
	
	return val;
}

/** Encode integer to string (base 10)
 *
 *
 * @param value Integer to encode.
 *
 * @return Encoded string.
 *
 */
static string decEncode(const long int value)
{
	ostringstream stream;
	
	stream << value;
	return stream.str();
}

/** Add leading zeroes to the string
 *
 * If the first character is not a digit then no leading zeroes
 * are added to the string.
 *
 * @param str String to add leading zeroes to.
 * @param num Number of leading zeroes (default: 2).
 *
 * @return String with leading zeroes.
 *
 */
static string leadzero(const string &str, const unsigned int num = 2)
{
	/* First character is not a digit */
	if ((str.length() > 0) && ((str[0] < '0') || (str[0] > '9')))
		return str;
	
	string ret = str;
	while (ret.length() < num)
		ret = string("0") + ret;
	
	return ret;
}

/** Decode month number from abbreviation
 *
 * Throws invalid_argument on invalid
 * month abbreviation.
 *
 * @param month Month abbreviation.
 *
 * @return Month number (1-based).
 *
 */
static long int monthDecode(const string &month)
{
	if (month == "Jan")
		return 1;
	
	if (month == "Feb")
		return 2;
	
	if (month == "Mar")
		return 3;
	
	if (month == "Apr")
		return 4;
	
	if (month == "May")
		return 5;
		
	if (month == "Jun")
		return 6;
	
	if (month == "Jul")
		return 7;
		
	if (month == "Aug")
		return 8;
	
	if (month == "Sep")
		return 9;
	
	if (month == "Oct")
		return 10;
	
	if (month == "Nov")
		return 11;
	
	if (month == "Dec")
		return 12;
	
	throw invalid_argument("Invalid month '" + month + "'");
}

/** Get first occurence of a character
 *
 * @param str   String to search in.
 * @param delim Character to search.
 * @param start Index to start searching from (default: 0).
 *
 * @return Index of the first occurence of the delimiting
 *         character.
 * @return Length of the string if the delimiting character
 *         is not found.
 *
 */
static string::size_type find_first(const string &str, const char delim,
    const string::size_type start = 0)
{
	for (string::size_type pos = start; pos < str.length(); pos++) {
		/* Found 'delim' */
		if (str[pos] == delim)
			return pos;
	}
	
	return str.length();
}

/** Find first non-occurence of a character
 *
 * @param str   String to search in.
 * @param until Character to avoid.
 * @param start Index to start searching from (default: 0).
 *
 * @return Index of the first occurence of a character different
 *         from the character we want to avoid.
 * @return Length of the string if the entire string comprises
 *         of the character we want to avoid.
 *
 */
static string::size_type find_until(const string &str, const char until,
    const string::size_type start = 0)
{
	for (string::size_type pos = start; pos < str.length(); pos++) {
		/* Found character which is not 'until' */
		if (str[pos] != until)
			return pos;
	}
	
	return str.length();
}

/** Split domain name into parts
 *
 * @param domain Domain name to tokenize.
 *
 * @return Tokenized domain name.
 *
 */
static domain_vector split_domain(const string &domain)
{
	domain_vector vector;
	
	/* Split string by '.' character */
	separator_type separator(".", "", keep_empty_tokens);
	tokenizer_type domain_tokens(domain, separator);
	
	for (tokenizer_type::iterator it = domain_tokens.begin();
	    it != domain_tokens.end(); ++it)
		vector.push_back(*it);
	
	return vector;
}

/** Extract date & time from log entry
 *
 * Throws invalid_argument on invalid
 * or no date & time signature.
 *
 * @param entry Date & time log entry.
 *
 * @return Decoded date & time.
 *
 */
static datetime extract_datetime(const string &entry)
{
	/* Date & time signature: [DD-Mon-YYYY:HH:MM:SS +off] */
	regex expression("\\[../.../....:..:..:.. .....\\]");
	
	string::const_iterator begin = entry.begin();
	string::const_iterator end = entry.end();
	match_results< string::const_iterator> match;
	
	datetime res;
	bool valid = false;
	
	/* Regexp match */
	if (regex_search(begin, end, match, expression, match_default)) {
		string datetime = match[0];
		
		/* Split match by all separating characters */
		separator_type separator("[/: ]", "", drop_empty_tokens);
		tokenizer_type datetime_tokens(datetime, separator);
		
		unsigned int pos = 0;
		for (tokenizer_type::iterator it = datetime_tokens.begin();
		    it != datetime_tokens.end(); ++it, pos++) {
			switch (pos) {
			case 0:  /* Day */
				res.day = decDecode(*it);
				break;
			case 1:  /* Month */
				res.month = monthDecode(*it);
				break;
			case 2: /* Year */
				res.year = decDecode(*it);
				break;
			case 3: /* Hour */
				res.hour = decDecode(*it);
				break;
			case 4: /* Minute */
				res.minute = decDecode(*it);
				break;
			case 5: /* Second */
				res.second = decDecode(*it);
				break;
			case 6: /* Offset */
				res.offset = decDecode(*it);
				valid = true;
				break;
			default:
				throw invalid_argument("Invalid date & time format");
			}
		}
	}
	
	if (valid)
		return res;
	
	throw invalid_argument("Date & time not found or not complete");
}

/** Long write (wrapper for write(2))
 *
 * @param fd    File descriptor.
 * @param buf   Data to write.
 * @param count Number of bytes to write.
 *
 */
static void write_long(int fd, const void *buf, size_t count)
{
	size_t total = count;
	
	while (total > 0) {
		ssize_t written = write(fd, buf, total);
		
		if (written < 0)
			return;
		
		total -= written;
		buf = (void *) (((char *) buf) + written);
	}
}

/** Process log entry and store to domain log
 *
 * @param entry Log entry to process.
 *
 */
static void process_entry(const string &entry)
{
	/* Ignore leading spaces */
	string::size_type domain_start = find_until(entry, ' ');
	
	/* Find domain name end */
	string::size_type domain_end = find_first(entry, ' ', domain_start);
	
	/* Ignore leading spaces in log entry */
	string::size_type log_start = find_until(entry, ' ', domain_end);
	
	/* Domain name is not empty */
	if ((log_start < entry.length()) && (domain_start < domain_end)) {
		string domain =
		    entry.substr(domain_start, domain_end - domain_start);
		domain_vector domain_parts = split_domain(domain);
		
		/* Domain name has two or more parts */
		if (domain_parts.size() >= 2) {
			string access = entry.substr(log_start);
			datetime log_time = extract_datetime(access);
			
			/*
			 * Domain log path is
			 * ${PREFIX}/${2ND_LEVEL_DOMAIN}/logs/${YYYY}-${MM}/${DOMAIN}${SUFFIX}
			 */
			string log_dir = prefix +
			    string("/") + domain_parts[domain_parts.size() - 2] +
			    string(".") + domain_parts[domain_parts.size() - 1] +
			    string("/logs/") + leadzero(decEncode(log_time.year), 4) +
			    string("-") + leadzero(decEncode(log_time.month)) +
			    suffix;
			
			/* Make sure the {YYYY}-{MM} directory exists */
			mkdir(log_dir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR |
			    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
			
			/* Open domain log for appending */
			int fd = open((log_dir + string("/") + domain).c_str(),
			    O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE,
			    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if (fd >= 0) {
				/* Store log entry */
				write_long(fd, access.c_str(), access.length());
				write_long(fd, "\n", 1);
				close(fd);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	/* Get optional suffix */
	if (argc > 1) {
		string arg = argv[1];
		regex filter("[a-z]*");
		string::const_iterator begin = arg.begin();
		string::const_iterator end = arg.end();
		match_results< string::const_iterator> match;
		
		if (regex_search(begin, end, match, filter, match_default))
			suffix = string(".") + match[0];
	}
	
	string entry;
	
	/* Process each line of input */
	while (getline(cin, entry, '\n')) {
		try {
			process_entry(entry);
		} catch (std::exception & e) {
			cerr << "Exception while processing access log entry: " <<
			    e.what() << endl;
		} catch (...) {
			/* All exceptions are treated non-fatal */
			cerr << "Unexpected exception while processing "
			    "access log entry" << endl;
		}
	}
	
	return 0;
}
