using System;
using System.Runtime.InteropServices;

public static class WordRenderer
{
    public static int ExportPdf(string inputPath, string outputPath)
    {
        Type wordType = Type.GetTypeFromProgID("Word.Application");
        if (wordType == null) throw new InvalidOperationException("Microsoft Word is not installed.");

        dynamic word = null;
        dynamic doc = null;
        try
        {
            word = Activator.CreateInstance(wordType);
            word.Visible = false;
            word.DisplayAlerts = 0;
            word.AutomationSecurity = 3;
            doc = word.Documents.Open(
                FileName: inputPath,
                ConfirmConversions: false,
                ReadOnly: true,
                AddToRecentFiles: false,
                Visible: false,
                OpenAndRepair: true,
                NoEncodingDialog: true);
            if (doc == null && (int)word.Documents.Count > 0)
            {
                doc = word.Documents.Item((int)word.Documents.Count);
            }
            if (doc == null) throw new InvalidOperationException("Word did not return an opened document.");
            doc.ExportAsFixedFormat(OutputFileName: outputPath, ExportFormat: 17);
            return (int)doc.ComputeStatistics(2);
        }
        finally
        {
            if (doc != null)
            {
                doc.Close(false);
                Marshal.FinalReleaseComObject(doc);
            }
            if (word != null)
            {
                word.Quit(false);
                Marshal.FinalReleaseComObject(word);
            }
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }
    }

    public static int UpdateAndExportPdf(string inputPath, string outputPath)
    {
        Type wordType = Type.GetTypeFromProgID("Word.Application");
        if (wordType == null) throw new InvalidOperationException("Microsoft Word is not installed.");

        dynamic word = null;
        dynamic doc = null;
        try
        {
            word = Activator.CreateInstance(wordType);
            word.Visible = false;
            word.DisplayAlerts = 0;
            word.AutomationSecurity = 3;
            Console.WriteLine("opening");
            dynamic opened = word.Documents.Open(
                FileName: inputPath,
                ConfirmConversions: false,
                ReadOnly: false,
                AddToRecentFiles: false,
                Revert: false,
                Visible: false,
                OpenAndRepair: true,
                NoEncodingDialog: true);
            doc = opened;
            if (doc == null && (int)word.Documents.Count > 0)
            {
                doc = word.Documents.Item((int)word.Documents.Count);
            }
            Console.WriteLine("opened");
            if (doc == null) throw new InvalidOperationException("Word did not return an opened document.");
            dynamic contentRange = doc.Content;
            if (contentRange != null && contentRange.Fields != null)
            {
                contentRange.Fields.Update();
            }
            Console.WriteLine("body-fields");
            int tocCount = (int)doc.TablesOfContents.Count;
            for (int i = 1; i <= tocCount; i++)
            {
                doc.TablesOfContents.Item(i).Update();
            }
            Console.WriteLine("toc");
            int sectionCount = (int)doc.Sections.Count;
            for (int si = 1; si <= sectionCount; si++)
            {
                dynamic section = doc.Sections.Item(si);
                int headerCount = (int)section.Headers.Count;
                for (int hi = 1; hi <= headerCount; hi++)
                {
                    dynamic header = section.Headers.Item(hi);
                    if ((bool)header.Exists && header.Range != null && header.Range.Fields != null)
                    {
                        header.Range.Fields.Update();
                    }
                }
                int footerCount = (int)section.Footers.Count;
                for (int fi = 1; fi <= footerCount; fi++)
                {
                    dynamic footer = section.Footers.Item(fi);
                    if ((bool)footer.Exists && footer.Range != null && footer.Range.Fields != null)
                    {
                        footer.Range.Fields.Update();
                    }
                }
            }
            Console.WriteLine("header-footer");
            doc.Repaginate();
            doc.Save();
            Console.WriteLine("updated");
            doc.ExportAsFixedFormat(OutputFileName: outputPath, ExportFormat: 17);
            Console.WriteLine("exported");
            return (int)doc.ComputeStatistics(2);
        }
        finally
        {
            if (doc != null)
            {
                doc.Close(false);
                Marshal.FinalReleaseComObject(doc);
            }
            if (word != null)
            {
                word.Quit(false);
                Marshal.FinalReleaseComObject(word);
            }
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }
    }
}
