import { useState } from "react";
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogFooter,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
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
    const banks = Array.from({ length: 10 }, (_, b) => ({
      bank: b,
      slots: Array.from({ length: 10 }, (_, s) => {
        const p = presets[b * 10 + s];
        if (!p) return null;
        return {
          slot: s,
          name: p.name,
          rawData: Array.from(p.rawData),
        };
      }).filter(Boolean),
    }));

    const blob = new Blob(
      [JSON.stringify({ version: 1, device: "multi-fx", banks }, null, 2)],
      { type: "application/json" }
    );
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement("a");
    a.href     = url;
    a.download = "presets.multifx";
    a.click();
    URL.revokeObjectURL(url);
    onClose();
  };

  const handleImport = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (evt) => {
      try {
        const json = JSON.parse(evt.target?.result as string);
        if (json.version !== 1 || json.device !== "multi-fx") {
          setImportError("Not a valid .multifx file");
          return;
        }
        const next: (PresetData | null)[] = Array(100).fill(null);
        for (const bankObj of json.banks) {
          for (const slot of bankObj.slots) {
            if (!slot) continue;
            const idx = bankObj.bank * 10 + slot.slot;
            next[idx] = {
              bank: bankObj.bank,
              slot: slot.slot,
              name: slot.name,
              rawData: new Uint8Array(slot.rawData),
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
    <Dialog open={open} onOpenChange={(v) => !v && onClose()}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Export / Import Presets</DialogTitle>
        </DialogHeader>

        <div className="space-y-4 py-2">
          <div>
            <Label>Export all presets to .multifx file</Label>
            <Button className="mt-2 w-full" onClick={handleExport}>
              Download presets.multifx
            </Button>
          </div>

          <div>
            <Label>Import from .multifx file</Label>
            <Input
              className="mt-2"
              type="file"
              accept=".multifx,application/json"
              onChange={handleImport}
            />
            {importError && (
              <p className="text-xs text-red-500 mt-1">{importError}</p>
            )}
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
