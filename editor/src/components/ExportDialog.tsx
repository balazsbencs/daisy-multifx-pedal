import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Dialog, DialogContent, DialogFooter, DialogHeader, DialogTitle } from "@/components/ui/dialog";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { PresetData } from "../hooks/useMidi";

interface ExportDialogProps {
  open: boolean;
  onClose: () => void;
  presets: (PresetData | null)[];
  onImportDone: (presets: (PresetData | null)[]) => void;
}

export function ExportDialog({ open, onClose, presets, onImportDone }: ExportDialogProps) {
  const [importError, setImportError] = useState("");

  const handleExport = () => {
    const banks = Array.from({ length: 10 }, (_, bank) => ({
      bank,
      slots: Array.from({ length: 10 }, (_, slot) => {
        const preset = presets[bank * 10 + slot];
        return preset
          ? {
              slot,
              name: preset.name,
              rawData: Array.from(preset.rawData),
            }
          : null;
      }).filter(Boolean),
    }));

    const blob = new Blob([JSON.stringify({ version: 1, device: "multi-fx", banks }, null, 2)], {
      type: "application/json",
    });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = "presets.multifx";
    link.click();
    URL.revokeObjectURL(url);
    onClose();
  };

  const handleImport = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (evt) => {
      try {
        const json = JSON.parse(String(evt.target?.result ?? ""));
        if (json.version !== 1 || json.device !== "multi-fx" || !Array.isArray(json.banks)) {
          setImportError("Not a valid .multifx file");
          return;
        }

        const next: (PresetData | null)[] = Array(100).fill(null);
        for (const bankObj of json.banks) {
          if (!Number.isInteger(bankObj.bank) || !Array.isArray(bankObj.slots)) continue;
          for (const slotObj of bankObj.slots) {
            if (!slotObj || !Number.isInteger(slotObj.slot) || !Array.isArray(slotObj.rawData)) continue;
            const idx = bankObj.bank * 10 + slotObj.slot;
            if (idx < 0 || idx >= 100 || slotObj.rawData.length !== 92) continue;
            next[idx] = {
              bank: bankObj.bank,
              slot: slotObj.slot,
              name: String(slotObj.name ?? ""),
              rawData: new Uint8Array(slotObj.rawData),
            };
          }
        }

        onImportDone(next);
        setImportError("");
        onClose();
      } catch {
        setImportError("Failed to parse file");
      }
    };
    reader.readAsText(file);
  };

  return (
    <Dialog open={open} onOpenChange={(value) => !value && onClose()}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Export / Import Presets</DialogTitle>
        </DialogHeader>

        <div className="space-y-5 py-2">
          <div>
            <Label>Export synced presets to a .multifx file</Label>
            <Button className="mt-2 w-full" onClick={handleExport}>
              Download presets.multifx
            </Button>
          </div>

          <div>
            <Label>Import from .multifx file</Label>
            <Input className="mt-2" type="file" accept=".multifx,application/json" onChange={handleImport} />
            {importError ? <p className="mt-1 text-xs text-red-400">{importError}</p> : null}
          </div>
        </div>

        <DialogFooter>
          <Button variant="outline" onClick={onClose}>
            Close
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
