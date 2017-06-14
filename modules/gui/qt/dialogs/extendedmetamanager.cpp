/*****************************************************************************
 * extendedmetamanager.cpp : Extended Metadata Manager dialogs
 ****************************************************************************
 * Copyright (C) 2017 Asier Santos Valcárcel
 * Authors: Asier Santos Valcárcel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/extendedmetamanager.hpp"
#include "components/interface_widgets.hpp"     /* CoverArtLabelExt */
#include "dialogs/fingerprintdialog.hpp"        /* fingerprinting dialog */
#include "adapters/chromaprint.hpp"             /* fingerprinting adapter (no UI)*/

#include <QMessageBox> /* for the Help and About popups */

#define UNUSED(x) (void)(x) //FIXME: delete this. Unused variable warning removal

/* Constructor */
ExtMetaManagerDialog::ExtMetaManagerDialog( intf_thread_t *_p_intf)
               : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    msg_Dbg( p_intf, "[ExtMetaManagerDialog] Initializing" );

    /* Basic UI setup */
    ui.setupUi( this ); //setup the UI from de compiled (.h) version of de QT ui (.ui)
    setWindowFlags( Qt::Tool );
    setWindowRole( "vlc-ext-meta-manager" );
    setWindowTitle( qtr( "Extended Metadata Manager" ) );

    /* Set button icons */
    ui.pushButton_getFromPlaylist->setIcon(QIcon(QPixmap (":/toolbar/playlist") ) );
    ui.pushButton_getFromFolder->setIcon(QIcon(QPixmap (":/type/folder-grey") ) );
    ui.pushButton_help->setIcon(QIcon(QPixmap (":/menu/help") ) );
    ui.pushButton_about->setIcon(QIcon(QPixmap (":/menu/info") ) );
    ui.pushButton_searchNow->setIcon(QIcon(QPixmap (":/") ) );
    ui.pushButton_saveAll->setIcon(QIcon(QPixmap (":/") ) );
    ui.pushButton_restoreAll->setIcon(QIcon(QPixmap (":/buttons/playlist/repeat_all") ) );
    ui.pushButton_clearTable->setIcon(QIcon(QPixmap (":") ) );
    ui.pushButton_cancel->setIcon(QIcon(QPixmap (":/menu/exit") ) );

    /* Button bindings */
    BUTTONACT( ui.pushButton_getFromPlaylist, getFromPlaylist() );
    BUTTONACT( ui.pushButton_getFromFolder, getFromFolder() );
    BUTTONACT( ui.pushButton_help, helpDialog() );
    BUTTONACT( ui.pushButton_about, aboutDialog() );
    BUTTONACT( ui.pushButton_searchNow, searchNow() );
    BUTTONACT( ui.pushButton_saveAll, saveAll() );
    BUTTONACT( ui.pushButton_restoreAll, restoreAll() );
    BUTTONACT( ui.pushButton_clearTable, clearTable() );
    BUTTONACT( ui.pushButton_cancel, close() );

    /* Events for the table */
    CONNECT( ui.tableWidget_metadata, cellClicked(int, int), this, updateArtwork(int, int) );
    CONNECT( ui.tableWidget_metadata,  itemChanged(QTableWidgetItem*), this,  multipleItemsChanged(QTableWidgetItem*));

    /* Set de table columns' size */
    ui.tableWidget_metadata->setColumnWidth(COL_CHECKBOX, 30);
    ui.tableWidget_metadata->setColumnWidth(COL_TITLE, 200);
    ui.tableWidget_metadata->setColumnWidth(COL_ARTIST, 200);
    ui.tableWidget_metadata->setColumnWidth(COL_ALBUM, 200);
    ui.tableWidget_metadata->setColumnWidth(COL_GENRE, 120);
    ui.tableWidget_metadata->setColumnWidth(COL_TRACKNUM, 70);
    ui.tableWidget_metadata->setColumnWidth(COL_PUBLISHER, 120);
    ui.tableWidget_metadata->setColumnWidth(COL_COPYRIGHT, 120);
    ui.tableWidget_metadata->setColumnWidth(COL_ARTWORK, 70);
    ui.tableWidget_metadata->setColumnWidth(COL_PATH, 50);

    /* Add the Artwork label */
    art_cover = new CoverArtLabelExt( this, p_intf );
    ui.gridLayout_artwork->layout()->addWidget(art_cover);

    /* Initilize the array for the currently working items */
    workspace = new vlc_array_t();
    vlc_array_init(workspace);

    /* Initilize our Boolean al false (meaning the playlist is not loaded) */
    playlistLoaded=false;

    /* Start with the progressBar disabled */
    ui.progressBar_search->setEnabled(false);

    QVLCTools::restoreWidgetPosition( p_intf, "ExtMetaManagerDialog", this );
}
/* Destructor */
ExtMetaManagerDialog::~ExtMetaManagerDialog()
{
    QVLCTools::saveWidgetPosition( p_intf, "ExtMetaManagerDialog", this );
}

/* Override the closing (click X) event*/
void ExtMetaManagerDialog::closeEvent(QCloseEvent *event)
{
    UNUSED(event); //FIXME: delete this
    close();
}

/* Show/hide */
void ExtMetaManagerDialog::toggleVisible()
{
    QVLCDialog::toggleVisible();
    if(isVisible()) //If changed to shown
        activateWindow();
}

/*----------------------------------------------------------------------------*/
/*-----------------------------Main use cases---------------------------------*/
/*----------------------------------------------------------------------------*/

/* Just closes the window (and the module itself) */
void ExtMetaManagerDialog::close()
{
    toggleVisible();

    /* Clean before closing */
    cleanUp();

    msg_Dbg( p_intf, "[ExtMetaManagerDialog] Closing" );
}

/* Loads files into the table from the current playlist */
void ExtMetaManagerDialog::getFromPlaylist()
{
    /* Clean before changing the workspace */
    cleanUp();

    /* Lock the playlist so we can work with it */
    playlist_Lock(THEPL);

    /* Get the size of the playlist and if no files selected, finish */
    int size = THEPL->items.i_size;
    if( size ==0 ) return;

    input_item_t *p_item;  //This is where each item will be stored
    int row; //This is where each item's position will be stored

    for(int i = 4;  i <= size+3; i++) //the list starts at 4 because the first 3 are not files
    {
        p_item = playlist_ItemGetById(THEPL, i)->p_input; // Get the playlist_item's input_item_t
        addTableEntry(p_item); //add item to the table

        /*Now we get the size of the table and store the item on that position
        on the workspace array, so item at row X on the table is also stored at
        array position X*/
        row = ui.tableWidget_metadata->rowCount();
        vlc_array_insert(workspace, p_item, row-1);
    }

    /* Unlock the playlist */
    playlist_Unlock(THEPL);

    /* Select the first cell and update artwork label */
    ui.tableWidget_metadata->setCurrentCell(0,1);
    updateArtwork(0,0);

    /* Playlist has been loaded, so we update our boolean */
    playlistLoaded=true;
}

/* Loads files into the table from a file explorer window */
void ExtMetaManagerDialog::getFromFolder()
{
    /* Open a file explorer just with audio files */
    QStringList uris = THEDP->showSimpleOpen(
        qtr("Open audio files to manage"),
        EXT_FILTER_AUDIO,
        p_intf->p_sys->filepath );

    /* If no files selected, finish */
    if( uris.isEmpty() ) return;

    /* Clean before changing the workspace */
    cleanUp();

    int row; //This is where each item's position will be stored

    foreach( const QString &uri, uris )
    {
        // Get the item from the URI
        input_item_t *p_item = getItemFromURI(uri.toLatin1().constData());
        addTableEntry(p_item); //Add the item to the table

        /*Now we get the size of the table and store the item on that position
        on the workspace array, so item at row X on the table is also stored at
        array position X*/
        row = ui.tableWidget_metadata->rowCount();
        vlc_array_insert(workspace, p_item, row-1);
    }

    /* Select the first cell and update artwork label */
    ui.tableWidget_metadata->setCurrentCell(0,1);
    updateArtwork(0,0);

    /* Files has been loaded, so we update our boolean */
    playlistLoaded=false;

}

/* Initiates the metadata search and analysis based on choosed options. It just
redirects to other methods depending on the activated checkboxes */
void ExtMetaManagerDialog::searchNow()
{
    if (ui.checkBox_acousticid->isChecked())
    {
        if (ui.checkBox_disableFastSearch->isChecked())
        {
            fingerprintTable(false);
        }
        else
        {
            fingerprintTable(true);
        }
    }
    if (ui.checkBox_filenameAnalysis->isChecked()) //TODO: implement this
    {
        msg_Dbg( p_intf, "[ExtMetaManagerDialog] filenameAnalysis checked" ); //FIXME: delete this

    }
}

/* Saves changes permanently*/
void ExtMetaManagerDialog::saveAll()
{
    input_item_t *p_item; // This is where the current working item will be
    const char *title_text;
    const char *artist_text;
    const char *album_text;
    const char *genre_text;
    const char *trackNum_text;
    const char *publisher_text;
    const char *copyright_text;

    /* Iterate over all the items on the table */
    int rows = ui.tableWidget_metadata->rowCount();

    for(int row = 0;  row < rows; row++) //The list starts at 4 because the first 3 are not files
    {
        /* Check if the row is checked/selected and ignore if not */
        if (rowIsSelected(row))
        {
            p_item = getItemFromRow(row); //First we obtain the input_item from the row

            /* ---Extract information form the table---
            getting the reference to one columns info is tricky, because the
            pointer is only valid while the QString (got with text()) hasn't
            changed. This means every time you get info from a new row, you
            loose the pointers to the previous row. That's why here we get the
            info and store it inmediately*/
            title_text = ui.tableWidget_metadata->item(row,COL_TITLE)->text().toLocal8Bit().constData();
            input_item_SetTitle(p_item, title_text);
            artist_text = ui.tableWidget_metadata->item(row,COL_ARTIST)->text().toLocal8Bit().constData();
            input_item_SetArtist(p_item, artist_text);
            album_text = ui.tableWidget_metadata->item(row,COL_ALBUM)->text().toLocal8Bit().constData();
            input_item_SetAlbum(p_item, album_text);
            genre_text = ui.tableWidget_metadata->item(row,COL_GENRE)->text().toLocal8Bit().constData();
            input_item_SetGenre(p_item, genre_text);
            trackNum_text = ui.tableWidget_metadata->item(row,COL_TRACKNUM)->text().toLocal8Bit().constData();
            input_item_SetTrackNum(p_item, trackNum_text);
            publisher_text = ui.tableWidget_metadata->item(row,COL_PUBLISHER)->text().toLocal8Bit().constData();
            input_item_SetPublisher(p_item, publisher_text);
            copyright_text = ui.tableWidget_metadata->item(row,COL_COPYRIGHT)->text().toLocal8Bit().constData();
            input_item_SetCopyright(p_item, copyright_text);

            // Save the meta on the item
            input_item_WriteMeta( VLC_OBJECT(THEPL), p_item ); // Write the metas
        }
    }
}

/* Just reloads all the items of the table, discarding unsaved changes */
void ExtMetaManagerDialog::restoreAll()
{
    clearTable();

    input_item_t *p_item; // This is where the current working item will be

    /* Iterate over all the items in the workspace */
    int arraySize = vlc_array_count(workspace);
    for(int i = 0; i < arraySize; i++)
    {
        /* Get one item form the list and add it to the table */
        p_item = (input_item_t*)vlc_array_item_at_index(workspace, i);
        addTableEntry(p_item);
    }
}

/* Deletes all entries from the table (still can be recovered with restoreAll) */
void ExtMetaManagerDialog::clearTable()
{
    ui.tableWidget_metadata->clearContents();
    ui.tableWidget_metadata->setRowCount(0);
    art_cover->clear();
}

/*----------------------------------------------------------------------------*/
/*--------------------Metadata & input management-----------------------------*/
/*----------------------------------------------------------------------------*/

/* Initiates the fingerprint process for all the table. If "fast" is true, 1st
entry is applied automatically */
void ExtMetaManagerDialog::fingerprintTable( bool fast )
{
    input_item_t *p_item; // This is where the current working item will be

    /* Get the number of items we'll be working with */
    int rows = ui.tableWidget_metadata->rowCount();

    /* Calculate how much the progress bar will advance each step (progressBar
    goes from 0 to 100). Then progress variable is set to 0 and the widget is
    updated */
    int progress_unit= 100/rows;
    int progress=0;
    ui.progressBar_search->setValue(progress);

    /* We dont want the table to mess things while we update it, so que block
    its signals (this is caused because we edited "multipleItemsChanged"). */
    ui.tableWidget_metadata->blockSignals(true);

    /* if fast search is activated, initilize custom fingerprinter */
    if (fast)
    {
        t = new (std::nothrow) Chromaprint( p_intf );
        if ( !t )
        {
            return; // Error
        }
    }

    /* Iterate the table */
    for(int row = 0; row < rows; row++)
    {
        /* Check if the row is checked/selected and ignore if not */
        if (rowIsSelected(row))
        {
            /* Get the item from the current row */
            p_item = getItemFromRow(row);

            /* Fingerprint the item and wait for results */
            fingerprint(p_item, fast);

            /* Update the table with the new info */
            updateTableEntry(p_item, row);
        }
        /* Update the progress bar */
        progress=progress+progress_unit; // Increase the progress
        ui.progressBar_search->setValue(progress); // Update the progressBar
    }

    /* Lost decimals can cause the progress bar to not reach 100, so here is
    the fix */
    ui.progressBar_search->setValue(100); //
    ui.progressBar_search->setEnabled(false);

    /* If fast search is activated, delete the custom fingerprinter */
    if (fast)
    {
        if ( t ) delete t;
        if ( p_r ) fingerprint_request_Delete( p_r );
    }

    /* We have finished, so we unlock al the table's signals. */
    ui.tableWidget_metadata->blockSignals(false);
}

/* Initiates the fingerprint process just for one item. If "fast" is true, 1st
entry is applied automatically */
void ExtMetaManagerDialog::fingerprint(input_item_t *p_item, bool fast)
{
    if (fast)
    {
        /* Add the item to the finperprinter's queue */
        if ( t )
            t->enqueue( p_item );

        /* Wait for results */
        p_r = t->fetchResults();
        while (!p_r)
            p_r = t->fetchResults();

        /* Check if metadata was found and if not, exit */
        if ( vlc_array_count( & p_r->results.metas_array ) == 0 )
            return;

        /* Apply first option */
        t->apply( p_r, 0 ); //TODO: check if empty
    }
    else
    {
        //Create a fingerprint dialog and wait until it's closed
        FingerprintDialog dialog(this, p_intf, p_item);
        dialog.exec();
    }

}

/* Recovers the item on a certain row (from the table) */
input_item_t* ExtMetaManagerDialog::getItemFromRow(int row)
{
    /* Item at row X is stored at workspace postion X */
    input_item_t *p_item = (input_item_t*)vlc_array_item_at_index(workspace, row);
    return p_item;
}

/* Gets an item from an URI and preparses it (gets it's metadata) */
input_item_t* ExtMetaManagerDialog::getItemFromURI(const char* uri)
{
    /* Create the item from the given uri */
    input_item_t *p_item = input_item_New( uri, "Entry" ); //The name is not relevant

    /* Add to the playlist so it is preparsed (metadata is got) */
    playlist_AddInput( THEPL, p_item, false, true );

    /* Wait until already preparsed (metadata was added) */
    while (!input_item_IsPreparsed(p_item)) ;

    return p_item;
}

/*----------------------------------------------------------------------------*/
/*--------------------------Table management----------------------------------*/
/*----------------------------------------------------------------------------*/

/* Modify the table's behavior so multiple items can be edited at the same time
when more than one cell is selected. */
void ExtMetaManagerDialog::multipleItemsChanged( QTableWidgetItem *item )
{
	ui.tableWidget_metadata->blockSignals(true);
	QList<QTableWidgetItem*> selectedItems = ui.tableWidget_metadata->selectedItems();
	foreach(QTableWidgetItem* selectItem, selectedItems)
	{
		selectItem->setText(item->text());
	}
	ui.tableWidget_metadata->blockSignals(false);
}

/* Adds a row on the table with the metadata from a given item */
void ExtMetaManagerDialog::addTableEntry(input_item_t *p_item)
{
    /* Add one row to the table Initilize */
    int row =   ui.tableWidget_metadata->rowCount(); //Last row is always rowCount-1
    ui.tableWidget_metadata->insertRow(row);

    /* Create checkbox for the first column */
    QCheckBox  *checkbox = new QCheckBox ();
    checkbox->setChecked(1); // Set checked by default
    /* Insert the checkbox in the cell */
    ui.tableWidget_metadata->setCellWidget(row, COL_CHECKBOX, checkbox );

    /* Create button for the artwork update */
    QPushButton  *m_button = new QPushButton( qtr("Change"), this);
    connect(m_button, SIGNAL (released()), this, SLOT (changeArtwork()));
    /* Insert the button in the cell */
    ui.tableWidget_metadata->setCellWidget(row, COL_ARTWORK, m_button );

    updateTableEntry(p_item,row);
}

/* Updates/Writes a (given) row on the table with one itme's metadata */
void ExtMetaManagerDialog::updateTableEntry(input_item_t *p_item, int row)
{
    /* Get metadata information from item */
    char *title_text = input_item_GetTitle(p_item);
    char *artist_text = input_item_GetArtist(p_item);
    char *album_text = input_item_GetAlbum(p_item);
    char *genre_text = input_item_GetGenre(p_item);
    char *trackNum_text = input_item_GetTrackNum(p_item);
    char *publisher_text = input_item_GetPublisher(p_item);
    char *copyright_text = input_item_GetCopyright(p_item);
    char *uri_text = input_item_GetURI(p_item);

    /* Fill all the cells with the extracted info */
    ui.tableWidget_metadata->setItem(row, COL_TITLE, new QTableWidgetItem( title_text ));
    ui.tableWidget_metadata->setItem(row, COL_ARTIST, new QTableWidgetItem( artist_text ));
    ui.tableWidget_metadata->setItem(row, COL_ALBUM, new QTableWidgetItem( album_text ));
    ui.tableWidget_metadata->setItem(row, COL_GENRE, new QTableWidgetItem( genre_text ));
    ui.tableWidget_metadata->setItem(row, COL_TRACKNUM, new QTableWidgetItem( trackNum_text ));
    ui.tableWidget_metadata->setItem(row, COL_PUBLISHER, new QTableWidgetItem( publisher_text ));
    ui.tableWidget_metadata->setItem(row, COL_COPYRIGHT, new QTableWidgetItem( copyright_text ));
    ui.tableWidget_metadata->setItem(row, COL_PATH, new QTableWidgetItem( uri_text ));
    ui.tableWidget_metadata->item(row, COL_PATH)->setFlags(0); // Make the path not selectable/editable
}

/* Returns a true if that row is seleted (checkbox is seletedc) and a flase if not */
bool ExtMetaManagerDialog::rowIsSelected(int row)
{
    /* Check if the row's is checkbox checked */
    QCheckBox  *checkbox = (QCheckBox*) ui.tableWidget_metadata->cellWidget(row,COL_CHECKBOX);
    return checkbox->isChecked();
}

/*----------------------------------------------------------------------------*/
/*----------------------------Artwork management------------------------------*/
/*----------------------------------------------------------------------------*/

/* When a cell on the table is selected, this function changes the Artwork
label's content to the selected item's artwork */
void ExtMetaManagerDialog::updateArtwork(int row, int column)
{
    UNUSED(column); //FIXME: delete this
    /* Get the item form the row, decode it's Artwork and update it in the UI */
    art_cover->showArtUpdate( getItemFromRow(row) );
}

/* change the artwork of the currently selected item */
void ExtMetaManagerDialog::changeArtwork()
{
    art_cover->setArtFromFile();
}

/*----------------------------------------------------------------------------*/
/*-------------------------------Others---------------------------------------*/
/*----------------------------------------------------------------------------*/

/* Clears the playlist */
void ExtMetaManagerDialog::clearPlaylist()
{
    /* Lock the playlist so we can work with it */
    playlist_Lock(THEPL);

    input_item_t *p_item; // This is where the current working item will be
    playlist_item_t *pl_item;  // This is where the playlist item of the previous item will be

    int arraySize = vlc_array_count(workspace); //Number of items we are workin with

    for(int i = 0; i < arraySize; i++)
    {
        p_item = (input_item_t*)vlc_array_item_at_index(workspace, i); //Get one item form the list
        pl_item = playlist_ItemGetByInput( THEPL, p_item); //Get the playlist_item_t of the previous item
        playlist_NodeDelete(THEPL, pl_item, true); //Delete the item form the pl
    }

    /* We've finished , so unlock the playlist */
    playlist_Unlock(THEPL);
}


/* Cleans the playlist, clears the table, empties workspace */
void ExtMetaManagerDialog::cleanUp()
{
    /* Clear the table, the playlist (if files have been loaded to it) and the
    array with with the currently working items*/
    clearTable();
    if (!playlistLoaded)
        clearPlaylist();
    vlc_array_clear(workspace); // This last or the previous ones won't work
}

/* Launches the "Help" dialog */
void ExtMetaManagerDialog::helpDialog()
{
    QMessageBox::information(
      this,
      tr("Help - Extended Metadata Manager"), //Title
      tr(help_text) ); //Text
}

/* Launches the "About" dialog */
void ExtMetaManagerDialog::aboutDialog()
{
    QMessageBox::information(
      this,
      tr("About - Extended Metadata Manager"), //Title
      tr(about_text) ); //Text
}
