#ifndef __DAVCURSOR_H__
#define __DAVCURSOR_H__ "$Id: davCursor.h,v 1.2 2008-09-21 21:56:25 ericn Exp $"

/*
 * davCursor.h
 *
 * This header file declares the davCursor_t class, 
 * which is used to display a hardware cursor of
 * the specified color, width, and height at the 
 * specified screen position.
 *
 * Note that the colors are specified as palette
 * entries into the ROM palette. Refer to page 66
 * of the VPBE user's manual for choices (0 is black,
 * 0xFF is white).
 *
 * Change History : 
 *
 * $Log: davCursor.h,v $
 * Revision 1.2  2008-09-21 21:56:25  ericn
 * [davCursor] No copy constructor
 *
 * Revision 1.1  2008-06-24 23:32:48  ericn
 * [jsCursor] Add support for Davinci HW cursor
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

class davCursor_t {
public:
	enum color_e {
		BLACK = 0,
		GRAY  = 7,
		WHITE = 0xff
	};
	davCursor_t( unsigned w = 10, unsigned h = 10, unsigned color = WHITE );
	~davCursor_t( void );

	void setColor( unsigned color );
	unsigned getColor( void ) const ;

	void setPos( unsigned x, unsigned y );
	void getPos( unsigned &x, unsigned &y );

	unsigned getWidth( void ) const ;
	unsigned getHeight( void ) const ; 

	void setWidth( unsigned w );
	void setHeight( unsigned h );

	void activate(void);
	void deactivate(void);

	static davCursor_t *get(void);
	static void destroy();

private:
        davCursor_t( davCursor_t const & );
	int const fdMem_ ;
	void 	 *mem_ ;
	unsigned *reg_rectcur_ ;	// 0x01c72610
	unsigned *reg_curxp_ ;		// 0x01c72688
	unsigned *reg_curyp_ ;		// 0x01c7268C
	unsigned *reg_curxl_ ;		// 0x01c72690
	unsigned *reg_curyl_ ;		// 0x01c72694
};

#endif

