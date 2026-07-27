

int tdc_stim_data_extract_and_rshift(int Data, int bitPosition, int bitLength)
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
