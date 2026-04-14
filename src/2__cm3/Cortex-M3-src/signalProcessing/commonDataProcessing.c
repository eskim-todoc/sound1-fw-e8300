

int data_ExtractionAndRigthShift(int Data, int bitPosition, int bitLength)
{
    int bitSet = 0x01;
    int extractedData;
    while (bitLength != 1)
    {
        bitSet = (bitSet << 1) | 0x01;
        bitLength--;
    }

    extractedData = (Data >> bitPosition) & bitSet;

    return extractedData;
}

int data_clearBit(int Data, int bitPosition, int bitLength)
{
    int bitSet = 0x01;
    int bitClearMasker;
    int bitClearedData;

    while (bitLength != 1)
    {
        bitSet = (bitSet << 1) | 0x01;
        bitLength--;
    }

    bitClearMasker = ~(bitSet << bitPosition);

    bitClearedData = Data & bitClearMasker;

    return bitClearedData;
}
