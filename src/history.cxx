#include <algorithm>
#include <fstream>
#include <cstring>

#ifndef _WIN32

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#endif /* _WIN32 */

#include "history.hxx"
#include "utf8string.hxx"

using namespace std;

namespace replxx {

static int const REPLXX_DEFAULT_HISTORY_MAX_LEN( 1000 );

History::History( void )
	: _data()
	, _maxSize( REPLXX_DEFAULT_HISTORY_MAX_LEN )
	, _index( 0 )
	, _previousIndex( -2 )
	, _recallMostRecent( false )
	, _unique( true ) {
}

void History::add( UnicodeString const& line ) {
	if ( ( _maxSize > 0 ) && ( _data.empty() || ( line != _data.back() ) ) ) {
		if ( _unique ) {
			_data.erase( std::remove( _data.begin(), _data.end(), line ), _data.end() );
		}
		if ( size() > _maxSize ) {
			_data.erase( _data.begin() );
			if ( -- _previousIndex < -1 ) {
				_previousIndex = -2;
			}
		}
		_data.push_back( line );
	}
}

void History::save( std::string const& filename ) {
#ifndef _WIN32
	mode_t old_umask = umask( S_IXUSR | S_IRWXG | S_IRWXO );
	std::string lockName( filename + ".lock" );
	int fdLock( ::open( lockName.c_str(), O_CREAT | O_RDWR, 0600 ) );
	static_cast<void>( ::lockf( fdLock, F_LOCK, 0 ) == 0 );
#endif
	lines_t data( std::move( _data ) );
	load( filename );
	for ( UnicodeString const& h : data ) {
		add( h );
	}
	ofstream histFile( filename );
	if ( ! histFile ) {
		return;
	}
#ifndef _WIN32
	umask( old_umask );
	chmod( filename.c_str(), S_IRUSR | S_IWUSR );
#endif
	Utf8String utf8;
	for ( UnicodeString const& h : _data ) {
		if ( ! h.is_empty() ) {
			utf8.assign( h );
			histFile << utf8.get() << endl;
		}
	}
#ifndef _WIN32
	static_cast<void>( ::lockf( fdLock, F_ULOCK, 0 ) == 0 );
	::close( fdLock );
	::unlink( lockName.c_str() );
#endif
	return;
}

void History::load( std::string const& filename ) {
	ifstream histFile( filename );
	if ( ! histFile ) {
		return;
	}
	string line;
	while ( getline( histFile, line ).good() ) {
		string::size_type eol( line.find_first_of( "\r\n" ) );
		if ( eol != string::npos ) {
			line.erase( eol );
		}
		if ( ! line.empty() ) {
			add( UnicodeString( line ) );
		}
	}
	return;
}

void History::clear( void ) {
	_data.clear();
	_index = 0;
	_previousIndex = -2;
}

void History::set_max_size( int size_ ) {
	if ( size_ >= 0 ) {
		_maxSize = size_;
		int curSize( size() );
		if ( _maxSize < curSize ) {
			_data.erase( _data.begin(), _data.begin() + ( curSize - _maxSize ) );
		}
	}
}

void History::reset_pos( int pos_ ) {
	if ( pos_ == -1 ) {
		_index = size() - 1;
		_recallMostRecent = false;
	} else {
		_index = pos_;
	}
}

bool History::move( bool up_ ) {
	if ( _previousIndex != -2 && ! up_ ) {
		_index = _previousIndex; // emulate Windows down-arrow
	} else {
		_index += up_ ? -1 : 1;
	}
	_previousIndex = -2;
	if (_index < 0) {
		_index = 0;
		return ( false );
	} else if ( _index >= size() ) {
		_index = size() - 1;
		return ( false );
	}
	_recallMostRecent = true;
	return ( true );
}

void History::jump( bool start_ ) {
	_index = start_ ? 0 : size() - 1;
	_previousIndex = -2;
	_recallMostRecent = true;
}

bool History::common_prefix_search( UnicodeString const& prefix_, int prefixSize_, bool back_ ) {
	int direct( size() + ( back_ ? -1 : 1 ) );
	int i( ( _index + direct ) % _data.size() );
	while ( i != _index ) {
		if ( _data[i].starts_with( prefix_.begin(), prefix_.begin() + prefixSize_ ) ) {
			_index = i;
			_previousIndex = -2;
			_recallMostRecent = true;
			return ( true );
		}
		i += direct;
		i %= _data.size();
	}
	return ( false );
}

UnicodeString const& History::operator[] ( int idx_ ) const {
	return ( _data[ idx_ ] );
}

}

