#ifndef __Fln_FlowChartStream__
#define __Fln_FlowChartStream__

/*
* Copyright 2010-2018 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the FlowLine2 application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.info.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <Udb/Obj.h>
#include <Udb/Transaction.h>

namespace Epk
{
	class EpkStream
	{
	public:
		/* External Format
			
				Slot <ascii> = "FlowLineStream"
				Slot <ascii> = "0.1"
				Slot <DateTime> = now
				[ Internal Format ]
		*/
		/* Internal Format (names as Tags)

		Sequence of
			Block
				Sequence of
					Frame 'proc' | 'note' | 'fram' | 'func' | 'evt' | 'conn'
						Slot 'oid' = <oid>
						Slot 'id' = <string>
						Slot 'text' = <string>
						Slot 'posx' = <double>
						Slot 'posy' = <double>
						Slot 'w'	= <double>
						Slot 'h'	= <double>
						Slot 'ctyp'	= <uint8>
						[ Slot 'ali' = <uuid>
						[ OutlineStream Internal Format ]

						[ Block ]* // falls proc, rekursiv

				Sequence of 
					Frame 'flow'
						Slot 'from' = <oid>
						Slot 'to' = <oid>
						Slot 'nlst' = <bml>						

		*/

        static void exportProc( Stream::DataWriter&, const Udb::Obj& proc ); // kann Uuid erzeugen
		const QString& getError() const { return d_error; }
        Udb::Obj importProc( Stream::DataReader&, Udb::Obj& parent );
	private:
        static void writeObjTo( Stream::DataWriter&, const Udb::Obj& orig, const Udb::Obj &diagItem );
        Udb::Obj readItem( Stream::DataReader&, Udb::Obj& parent, quint64* remoteOid = 0, int level = 0 );
        Udb::Obj readFlow( Stream::DataReader&, Udb::Obj& parent, const QHash<quint64, Udb::Obj> &objs, int level );
        static void writeAtts( Stream::DataWriter&, const Udb::Obj& orig, const Udb::Obj& diagItem );
		QString d_error;
	};
}

#endif
