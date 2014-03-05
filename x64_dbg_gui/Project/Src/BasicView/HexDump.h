#ifndef DUMP_H
#define DUMP_H

#include <QtGui>
#include <QtDebug>
#include "NewTypes.h"
#include "AbstractTableView.h"
#include "MemoryPage.h"
#include "QBeaEngine.h"
#include "Bridge.h"
#include <sstream>

class HexDump : public AbstractTableView
{
    Q_OBJECT
public:
    enum DataSize_e
    {
        Byte,
        Word,
        Dword,
        Qword,
        Tword
    };

    enum ByteViewMode_e
    {
        HexByte,
        AsciiByte,
        SignedDecByte,
        UnsignedDecByte
    };

    enum WordViewMode_e
    {
        HexWord,
        UnicodeWord,
        SignedDecWord,
        UnsignedDecWord
    };

    enum DwordViewMode_e
    {
        HexDword,
        SignedDecDword,
        UnsignedDecDword,
        FloatDword //sizeof(float)=4
    };

    enum QwordViewMode_e
    {
        HexQword,
        SignedDecQword,
        UnsignedDecQword,
        DoubleQword //sizeof(double)=8
    };

    enum TwordViewMode_e
    {
        FloatTword
    };

    typedef struct _DataDescriptor_t
    {
        DataSize_e itemSize;            // Items size
        union                       // View mode
        {
            ByteViewMode_e byteMode;
            WordViewMode_e wordMode;
            DwordViewMode_e dwordMode;
            QwordViewMode_e qwordMode;
            TwordViewMode_e twordMode;
        };
    } DataDescriptor_t;

    typedef struct _ColumnDescriptor_t
    {
        bool isData;
        int itemCount;
        DataDescriptor_t data;
    } ColumnDescriptor_t;

    explicit HexDump(QWidget *parent = 0);

    //QString getStringToPrint(int rowBase, int rowOffset, int col);
    void mouseMoveEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);

    QString paintContent(QPainter* painter, int_t rowBase, int rowOffset, int col, int x, int y, int w, int h);
    void paintGraphicDump(QPainter* painter, int x, int y, int addr);

    void printSelected(QPainter* painter, int_t rowBase, int rowOffset, int col, int x, int y, int w, int h);

    // Selection Management
    void expandSelectionUpTo(int_t rva);
    void setSingleSelection(int_t rva);
    int getInitialSelection();
    bool isSelected(int_t rva);

    QString getString(int col, int_t rva);
    int getSizeOf(DataSize_e size);

    QString toString(DataDescriptor_t desc, void *data);

    QString byteToString(byte_t byte, ByteViewMode_e mode);
    QString wordToString(uint16 word, WordViewMode_e mode);
    QString dwordToString(uint32 dword, DwordViewMode_e mode);
    QString qwordToString(uint64 qword, QwordViewMode_e mode);
    QString twordToString(long double tword, TwordViewMode_e mode);

    int getStringMaxLength(DataDescriptor_t desc);

    int byteStringMaxLength(ByteViewMode_e mode);
    int wordStringMaxLength(WordViewMode_e mode);
    int dwordStringMaxLength(DwordViewMode_e mode);
    int qwordStringMaxLength(QwordViewMode_e mode);
    int twordStringMaxLength(TwordViewMode_e mode);

    int getItemIndexFromX(int x);
    int_t getItemStartingAddress(int x, int y);

    int getBytePerRowCount();
    int getItemPixelWidth(ColumnDescriptor_t desc);

    //descriptor management
    void appendDescriptor(int width, QString title, bool clickable, ColumnDescriptor_t descriptor);
    void clearDescriptors();
    
public slots:
    void printDumpAt(int_t parVA);
    void debugStateChanged(DBGSTATE state);

private:
    enum GuiState_t {NoState, MultiRowsSelectionState};

    typedef struct _RowDescriptor_t
    {
        int_t firstSelectedIndex;
        int_t fromIndex;
        int_t toIndex;
    } SelectionData_t;

    SelectionData_t mSelection;
    
    GuiState_t mGuiState;

protected:
    MemoryPage* mMemPage;
    int_t mBase;
    int_t mSize;
    int mByteOffset;
    QList<ColumnDescriptor_t> mDescriptor;
};

#endif // DUMP_H
