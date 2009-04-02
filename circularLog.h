#ifndef __CIRCULARLOG_H__
#define __CIRCULARLOG_H__ "$Id: circularLog.h,v 1.1 2009-04-02 20:32:53 ericn Exp $"

/*
 * circularLog.h
 *
 * This header file declares the circularLog_t class,
 * which can be used to log data to a limited-size file
 * for use in a RAM disk. 
 * 
 * After the initial fill of the log file, the class will
 * ensure that at least 3/4 of the specified file size is
 * kept at all times by shifting data earlier in the file
 * when necessary to provide space.
 *
 * Change History : 
 *
 * $Log: circularLog.h,v $
 * Revision 1.1  2009-04-02 20:32:53  ericn
 * [circularLog] Added module and test program
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2009
 */


class circularLog_t {
public:
	circularLog_t( char const *fileName, unsigned maxLen );
	~circularLog_t( void );

	bool isOpen(void) const { return 0 <= fd_ ; }
	void write( void const *data, unsigned len );

private:
	enum {
		NUMDATACHUNKS = 4
	};
	char const *fileName_ ;
	unsigned    chunkSize_ ;
	char	   *dataChunks_[NUMDATACHUNKS];
	unsigned    chunkIdx_ ;
	unsigned    curChunkLen_ ;
	int	    fd_ ;
	bool	    wrapped_ ;
};


#endif

