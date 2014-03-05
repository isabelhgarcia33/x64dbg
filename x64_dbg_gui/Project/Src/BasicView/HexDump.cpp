#include "HexDump.h"

HexDump::HexDump(QWidget *parent) : AbstractTableView(parent)
{
    SelectionData_t data;
    memset(&data, 0, sizeof(SelectionData_t));
    mSelection = data;

    mBase = 0;
    mSize = 0;

    mGuiState = HexDump::NoState;

    setRowCount(0);

    mMemPage = new MemoryPage(0, 0);

    clearDescriptors();

    connect(Bridge::getBridge(), SIGNAL(updateDump()), this, SLOT(reloadData()));
    connect(Bridge::getBridge(), SIGNAL(dbgStateChanged(DBGSTATE)), this, SLOT(debugStateChanged(DBGSTATE)));
}

void HexDump::printDumpAt(int_t parVA)
{
    int_t wBase = DbgMemFindBaseAddr(parVA, 0); //get memory base
    int_t wSize = DbgMemGetPageSize(wBase); //get page size
    int_t wRVA = parVA - wBase; //calculate rva
    int wBytePerRowCount = getBytePerRowCount(); //get the number of bytes per row
    int_t wRowCount;

    // Byte offset used to be aligned on the given RVA
    mByteOffset = (int)((int_t)wRVA % (int_t)wBytePerRowCount);
    mByteOffset = mByteOffset > 0 ? wBytePerRowCount - mByteOffset : 0;

    // Compute row count
    wRowCount = wSize / wBytePerRowCount;
    wRowCount += mByteOffset > 0 ? 1 : 0;

    setRowCount(wRowCount); //set the number of rows

    mMemPage->setAttributes(wBase, wSize);  // Set base and size (Useful when memory page changed)
    mBase = wBase;
    mSize = wSize;

    setTableOffset(-1); //make sure the requested address is always first

    setTableOffset((wRVA + mByteOffset) / wBytePerRowCount); //change the displayed offset
}

void HexDump::mouseMoveEvent(QMouseEvent* event)
{
    bool wAccept = true;

    if(mGuiState == HexDump::MultiRowsSelectionState)
    {
        //qDebug() << "State = MultiRowsSelectionState";

        if((transY(event->y()) >= 0) && event->y() <= this->height())
        {
            for(int wI = 1; wI < getColumnCount(); wI++)    // Skip first column (Addresses)
            {
                int wColIndex = getColumnIndexFromX(event->x());

                if(wColIndex > 0) // No selection for first column (addresses)
                {
                    int_t wStartingAddress = getItemStartingAddress(event->x(), event->y());
                    int_t wEndingAddress = wStartingAddress + getSizeOf(mDescriptor.at(wColIndex - 1).data.itemSize) - 1;

                    if(wEndingAddress < mSize)
                    {
                        if(wStartingAddress < getInitialSelection())
                            expandSelectionUpTo(wStartingAddress);
                        else
                            expandSelectionUpTo(wEndingAddress);

                        mGuiState = HexDump::MultiRowsSelectionState;

                        repaint();
                    }
                }
            }

            wAccept = true;
        }
    }

    if(wAccept == true)
        AbstractTableView::mouseMoveEvent(event);
}

void HexDump::mousePressEvent(QMouseEvent* event)
{
    //qDebug() << "HexDump::mousePressEvent";

    bool wAccept = false;

    if(((event->buttons() & Qt::LeftButton) != 0) && ((event->buttons() & Qt::RightButton) == 0))
    {
        if(getGuiState() == AbstractTableView::NoState)
        {
            if(event->y() > getHeaderHeight() && event->y() <= this->height())
            {
                int wColIndex = getColumnIndexFromX(event->x());

                for(int wI = 1; wI < getColumnCount(); wI++)    // Skip first column (Addresses)
                {
                    if(wColIndex > 0 && mDescriptor.at(wColIndex - 1).isData == true) // No selection for first column (addresses) and no data columns
                    {
                        int_t wStartingAddress = getItemStartingAddress(event->x(), event->y());
                        int_t wEndingAddress = wStartingAddress + getSizeOf(mDescriptor.at(wColIndex - 1).data.itemSize) - 1;

                        if(wEndingAddress < mSize)
                        {
                            setSingleSelection(wStartingAddress);
                            expandSelectionUpTo(wEndingAddress);

                            mGuiState = HexDump::MultiRowsSelectionState;

                            repaint();
                        }
                    }
                }

                wAccept = true;
            }
        }
    }

    if(wAccept == false)
        AbstractTableView::mousePressEvent(event);
}

void HexDump::mouseReleaseEvent(QMouseEvent* event)
{
    bool wAccept = true;

    if((event->buttons() & Qt::LeftButton) == 0)
    {
        if(mGuiState == HexDump::MultiRowsSelectionState)
        {
            mGuiState = HexDump::NoState;

            this->viewport()->repaint();

            wAccept = false;
        }
    }

    if(wAccept == true)
        AbstractTableView::mouseReleaseEvent(event);
}

QString HexDump::paintContent(QPainter* painter, int_t rowBase, int rowOffset, int col, int x, int y, int w, int h)
{
    // Reset byte offset when base address is reached
    if(rowBase == 0 && mByteOffset != 0)
        printDumpAt(mBase);

    // Compute RVA
    int wBytePerRowCount = getBytePerRowCount();
    int_t wRva = (rowBase + rowOffset) * wBytePerRowCount - mByteOffset;

    QString wStr = "";
    if(col == 0)    // Addresses
    {
        wStr += QString("%1").arg(mBase + wRva, sizeof(int_t) * 2, 16, QChar('0')).toUpper();
    }
    else if(mDescriptor.at(col - 1).isData == true) //paint data
    {
        printSelected(painter, rowBase, rowOffset, col, x, y, w, h);
        wStr += getString(col - 1, wRva);
    }
    else //paint non-data
    {

    }
    return wStr;
}

void HexDump::printSelected(QPainter* painter, int_t rowBase, int rowOffset, int col, int x, int y, int w, int h)
{
    if((col > 0) && ((col - 1) < mDescriptor.size()))
    {
        int wI = 0;
        int wBytePerRowCount = getBytePerRowCount();
        int_t wRva = (rowBase + rowOffset) * wBytePerRowCount - mByteOffset;
        int wItemPixWidth = getItemPixelWidth(mDescriptor.at(col - 1));
        int wSelectionX;
        int wSelectionWidth;

        for(wI = 0; wI < mDescriptor.at(col - 1).itemCount; wI++)
        {
            if(isSelected(wRva + wI * getSizeOf(mDescriptor.at(col - 1).data.itemSize)) == true)
            {
                wSelectionX = x + wI * wItemPixWidth;
                wSelectionWidth = wItemPixWidth > w - (wSelectionX - x) ? w - (wSelectionX - x) : wItemPixWidth;
                wSelectionWidth = wSelectionWidth < 0 ? 0 : wSelectionWidth;

                painter->fillRect(QRect(wSelectionX, y, wSelectionWidth, h), QBrush(QColor(192,192,192)));
            }
        }
    }
}

/************************************************************************************
                                Selection Management
************************************************************************************/
void HexDump::expandSelectionUpTo(int_t rva)
{
    if(rva < mSelection.firstSelectedIndex)
    {
        mSelection.fromIndex = rva;
        mSelection.toIndex = mSelection.firstSelectedIndex;
    }
    else if(rva > mSelection.firstSelectedIndex)
    {
        mSelection.fromIndex = mSelection.firstSelectedIndex;
        mSelection.toIndex = rva;
    }
}

void HexDump::setSingleSelection(int_t rva)
{
    mSelection.firstSelectedIndex = rva;
    mSelection.fromIndex = rva;
    mSelection.toIndex = rva;
}

int HexDump::getInitialSelection()
{
    return mSelection.firstSelectedIndex;
}

bool HexDump::isSelected(int_t rva)
{
    if(rva >= mSelection.fromIndex && rva <= mSelection.toIndex)
        return true;
    else
        return false;
}

QString HexDump::getString(int col, int_t rva)
{
    int wI;
    QString wStr = "";

    int wByteCount = getSizeOf(mDescriptor.at(col).data.itemSize);
    int wBufferByteCount = mDescriptor.at(col).itemCount * wByteCount;

    wBufferByteCount = wBufferByteCount > (mSize - rva) ? mSize - rva : wBufferByteCount;

    byte_t* wData = new byte_t[wBufferByteCount];
    //byte_t wData[mDescriptor.at(col).itemCount * wByteCount];

    mMemPage->readOriginalMemory(wData, rva, wBufferByteCount);

    for(wI = 0; wI < mDescriptor.at(col).itemCount && (rva + wI) < mSize; wI++)
    {
        if((rva + wI + wByteCount - 1) < mSize)
            wStr += toString(mDescriptor.at(col).data, (void*)(wData + wI * wByteCount)).rightJustified(getStringMaxLength(mDescriptor.at(col).data), ' ') + " ";
        else
            wStr += QString("?").rightJustified(getStringMaxLength(mDescriptor.at(col).data), ' ') + " ";
    }

    delete[] wData;

    return wStr;
}

QString HexDump::toString(DataDescriptor_t desc, void* data) //convert data to string
{
    QString wStr = "";

    switch(desc.itemSize)
    {
    case Byte:
    {
        wStr = byteToString(*((byte_t*)data), desc.byteMode);
    }
    break;

    case Word:
    {
        wStr = wordToString(*((uint16*)data), desc.wordMode);
    }
    break;

    case Dword:
    {
        wStr = dwordToString(*((uint32*)data), desc.dwordMode);
    }
    break;

    case Qword:
    {
        wStr = qwordToString(*((uint64*)data), desc.qwordMode);
    }
    break;

    case Tword:
    {
        //TODO: sizeof(long double)=12, not 10
        wStr = twordToString(*((long double*)data), desc.twordMode);
    }
    break;

    default:
    {

    }
    break;
    }

    return wStr;
}

QString HexDump::byteToString(byte_t byte, ByteViewMode_e mode)
{
    QString wStr = "";

    switch(mode)
    {
    case HexByte:
    {
        wStr = QString("%1").arg((unsigned char)byte, 2, 16, QChar('0')).toUpper();
    }
    break;

    case AsciiByte:
    {
        QChar wChar((char)byte);

        if(wChar.isPrint() == true)
            wStr = QString(wChar);
        else
            wStr = ".";
    }
    break;

    case SignedDecByte:
    {
        wStr = QString::number((int)((char)byte));
    }
    break;

    case UnsignedDecByte:
    {
        wStr = QString::number((unsigned int)byte);
    }
    break;

    default:
    {

    }
    break;
    }

    return wStr;
}

QString HexDump::wordToString(uint16 word, WordViewMode_e mode)
{
    QString wStr = "";

    switch(mode)
    {
    case HexWord:
    {
        wStr = QString("%1").arg((unsigned short)word, 4, 16, QChar('0')).toUpper();
    }
    break;

    case UnicodeWord:
    {
        QChar wChar((unsigned short)word);

        if(wChar.isPrint() == true)
            wStr = QString(wChar);
        else
            wStr = ".";
    }
    break;

    case SignedDecWord:
    {
        wStr = QString::number((int)((short)word));
    }
    break;

    case UnsignedDecWord:
    {
        wStr = QString::number((unsigned int)word);
    }
    break;

    default:
    {

    }
    break;
    }

    return wStr;
}

QString HexDump::dwordToString(uint32 dword, DwordViewMode_e mode)
{
    QString wStr = "";

    switch(mode)
    {
    case HexDword:
    {
        wStr = QString("%1").arg((unsigned int)dword, 8, 16, QChar('0')).toUpper();
    }
    break;

    case SignedDecDword:
    {
        wStr = QString::number((int)dword);
    }
    break;

    case UnsignedDecDword:
    {
        wStr = QString::number((unsigned int)dword);
    }
    break;

    case FloatDword:
    {
        float* wPtr = (float*)&dword;
        wStr = QString::number((double)*wPtr);
    }
    break;

    default:
    {

    }
    break;
    }

    return wStr;
}

QString HexDump::qwordToString(uint64 qword, QwordViewMode_e mode)
{
    QString wStr = "";

    switch(mode)
    {
    case HexQword:
    {
        wStr = QString("%1").arg((unsigned long long)qword, 16, 16, QChar('0')).toUpper();
    }
    break;

    case SignedDecQword:
    {
        wStr = QString::number((long long)qword);
    }
    break;

    case UnsignedDecQword:
    {
        wStr = QString::number((unsigned long long)qword);
    }
    break;

    case DoubleQword:
    {
        double* wPtr = (double*)&qword;
        wStr = QString::number((double)*wPtr);
    }
    break;

    default:
    {

    }
    break;
    }

    return wStr;
}

QString HexDump::twordToString(long double tword, TwordViewMode_e mode)
{
    QString wStr = "";

    switch(mode)
    {
    case FloatTword:
    {
        std::stringstream wlongDoubleStr;
        wlongDoubleStr <<  std::scientific << (long double)tword;

        wStr = QString::fromStdString(wlongDoubleStr.str());
    }
    break;

    default:
    {

    }
    break;
    }

    return wStr;
}

int HexDump::getSizeOf(DataSize_e size)
{
    int wSize = 0;

    switch(size)
    {
    case Byte:          // 1 Byte
    {
        wSize = 1;
    }
    break;

    case Word:          // 2 Bytes
    {
        wSize = 2;
    }
    break;

    case Dword:         // 4 Bytes
    {
        wSize = 4;
    }
    break;

    case Qword:         // 8 Bytes
    {
        wSize = 8;
    }
    break;

    case Tword:         // 10 Bytes
    {
        wSize = 10;
    }
    break;

    default:
    {
        wSize = 0;
    }
    }

    return wSize;
}

int HexDump::getStringMaxLength(DataDescriptor_t desc)
{
    int wLength = 0;

    switch(desc.itemSize)
    {
    case Byte:
    {
        wLength = byteStringMaxLength(desc.byteMode);
    }
    break;

    case Word:
    {
        wLength = wordStringMaxLength(desc.wordMode);
    }
    break;

    case Dword:
    {
        wLength = dwordStringMaxLength(desc.dwordMode);
    }
    break;

    case Qword:
    {
        wLength = qwordStringMaxLength(desc.qwordMode);
    }
    break;

    case Tword:
    {
        wLength = twordStringMaxLength(desc.twordMode);
    }
    break;

    default:
    {

    }
    break;
    }

    return wLength;
}

int HexDump::byteStringMaxLength(ByteViewMode_e mode)
{
    int wLength = 0;

    switch(mode)
    {
    case HexByte:
    {
        wLength = 2;
    }
    break;

    case AsciiByte:
    {
        wLength = 1;
    }
    break;

    case SignedDecByte:
    {
        wLength = 4;
    }
    break;

    case UnsignedDecByte:
    {
        wLength = 3;
    }
    break;

    default:
    {

    }
    break;
    }

    return wLength;
}

int HexDump::wordStringMaxLength(WordViewMode_e mode)
{
    int wLength = 0;

    switch(mode)
    {
    case HexWord:
    {
        wLength = 4;
    }
    break;

    case UnicodeWord:
    {
        wLength = 1;
    }
    break;

    case SignedDecWord:
    {
        wLength = 6;
    }
    break;

    case UnsignedDecWord:
    {
        wLength = 5;
    }
    break;

    default:
    {

    }
    break;
    }

    return wLength;
}

int HexDump::dwordStringMaxLength(DwordViewMode_e mode)
{
    int wLength = 0;

    switch(mode)
    {
    case HexDword:
    {
        wLength = 8;
    }
    break;

    case SignedDecDword:
    {
        wLength = 11;
    }
    break;

    case UnsignedDecDword:
    {
        wLength = 10;
    }
    break;

    case FloatDword:
    {
        wLength = 13;
    }
    break;

    default:
    {

    }
    break;
    }

    return wLength;
}

int HexDump::qwordStringMaxLength(QwordViewMode_e mode)
{
    int wLength = 0;

    switch(mode)
    {
    case HexQword:
    {
        wLength = 16;
    }
    break;

    case SignedDecQword:
    {
        wLength = 20;
    }
    break;

    case UnsignedDecQword:
    {
        wLength = 20;
    }
    break;

    case DoubleQword:
    {
        wLength = 23;
    }
    break;

    default:
    {

    }
    break;
    }

    return wLength;
}

int HexDump::twordStringMaxLength(TwordViewMode_e mode)
{
    int wLength = 0;

    switch(mode)
    {
    case FloatTword:
    {
        wLength = 29;
    }
    break;

    default:
    {

    }
    break;
    }

    return wLength;
}

int HexDump::getItemIndexFromX(int x)
{
    int wColIndex = getColumnIndexFromX(x);

    if(wColIndex > 0)
    {
        int wColStartingPos = getColumnPosition(wColIndex);
        int wRelativeX = x - wColStartingPos;

        int wItemPixWidth = getItemPixelWidth(mDescriptor.at(wColIndex - 1));

        int wItemIndex = wRelativeX / wItemPixWidth;

        wItemIndex = wItemIndex < 0 ? 0 : wItemIndex;
        wItemIndex = wItemIndex > (mDescriptor.at(wColIndex - 1).itemCount - 1) ? (mDescriptor.at(wColIndex - 1).itemCount - 1) : wItemIndex;

        return wItemIndex;
    }
    else
    {
        return 0;
    }
}

int_t HexDump::getItemStartingAddress(int x, int y)
{
    int wRowOffset = getIndexOffsetFromY(transY(y));
    int wItemIndex = getItemIndexFromX(x);
    int wColIndex = getColumnIndexFromX(x);
    int_t wStartingAddress = 0;

    if(wColIndex > 0)
    {
        wColIndex -= 1;
        wStartingAddress = (getTableOffset() + wRowOffset) * (mDescriptor.at(wColIndex).itemCount * getSizeOf(mDescriptor.at(wColIndex).data.itemSize)) + wItemIndex * getSizeOf(mDescriptor.at(wColIndex).data.itemSize) - mByteOffset;
    }

    return wStartingAddress;
}

int HexDump::getBytePerRowCount()
{
    return mDescriptor.at(0).itemCount * getSizeOf(mDescriptor.at(0).data.itemSize);
}

int HexDump::getItemPixelWidth(ColumnDescriptor_t desc)
{
    int wCharWidth = QFontMetrics(this->font()).width(QChar('C'));
    int wItemPixWidth = getStringMaxLength(desc.data) * wCharWidth + wCharWidth;

    return wItemPixWidth;
}

void HexDump::appendDescriptor(int width, QString title, bool clickable, ColumnDescriptor_t descriptor)
{
    addColumnAt(width, title, clickable);
    mDescriptor.append(descriptor);
}

void HexDump::clearDescriptors()
{
    deleteAllColumns();
    mDescriptor.clear();
    int charwidth=QFontMetrics(this->font()).width(QChar(' '));
    addColumnAt(8+charwidth*2*sizeof(uint_t), "Address", false); //address
}

void HexDump::debugStateChanged(DBGSTATE state)
{
    if(state==stopped)
    {
        mBase=0;
        mSize=0;
        setRowCount(0);
        reloadData();
    }
}
